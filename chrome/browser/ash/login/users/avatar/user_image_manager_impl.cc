// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/image_downloader.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/users/avatar/user_image_loader_delegate.h"
#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"
#include "chrome/browser/ash/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

// Delay between user login and attempt to update user's profile data.
constexpr int kProfileDataDownloadDelaySec = 10;

// Interval between retries to update user's profile data.
constexpr int kProfileDataDownloadRetryIntervalSec = 300;

// Delay between subsequent profile refresh attempts (24 hrs).
constexpr int kProfileRefreshIntervalSec = 24 * 3600;

static bool g_ignore_profile_data_download_delay_ = false;

static bool g_skip_profile_download = false;

static bool g_skip_default_user_image_download = false;

// Saves `image_bytes` at `image_path`, and delete the old file at
// `old_image_path` if needed.
bool SaveAndDeleteImage(scoped_refptr<base::RefCountedBytes> image_bytes,
                        const base::FilePath& image_path,
                        const base::FilePath& old_image_path) {
  if (image_bytes->size() == 0 ||
      !base::WriteFile(image_path, base::make_span(image_bytes->front(),
                                                   image_bytes->size()))) {
    LOG(ERROR) << "Failed to save image to file: " << image_path.AsUTF8Unsafe();
    return false;
  }
  if (!old_image_path.empty() && old_image_path != image_path) {
    if (!base::DeleteFile(old_image_path)) {
      LOG(ERROR) << "Failed to delete old image: "
                 << old_image_path.AsUTF8Unsafe();
      return false;
    }
  }

  return true;
}

// Returns the codec enum for the given image path's extension.
ImageDecoder::ImageCodec ChooseCodecFromPath(const base::FilePath& image_path) {
  if (image_path.Extension() == FILE_PATH_LITERAL(".png")) {
    return ImageDecoder::PNG_CODEC;
  }

  return ImageDecoder::DEFAULT_CODEC;
}

// Returns the suffix for the given image format, that should be JPEG or PNG.
const char* ChooseExtensionFromImageFormat(
    user_manager::UserImage::ImageFormat image_format) {
  switch (image_format) {
    case user_manager::UserImage::FORMAT_JPEG:
      return ".jpg";
    case user_manager::UserImage::FORMAT_PNG:
      return ".png";
    case user_manager::UserImage::FORMAT_WEBP:
      return ".webp";
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid format: " << image_format;
      return ".jpg";
  }
}

}  // namespace

// static
int UserImageManagerImpl::ImageIndexToHistogramIndex(int image_index) {
  switch (image_index) {
    case user_manager::UserImage::Type::kExternal:
      return default_user_image::kHistogramImageExternal;
    case user_manager::UserImage::Type::kProfile:
      return default_user_image::kHistogramImageFromProfile;
    default:
      return image_index + default_user_image::kHistogramSpecialImagesMaxCount;
  }
}

// static
void UserImageManagerImpl::RecordUserImageChanged(int histogram_value) {
  // Although |UserImageManagerImpl::kUserImageChangedHistogramName| is an
  // enumerated histogram, we intentionally use UmaHistogramExactLinear() to
  // emit the metric rather than UmaHistogramEnumeration(). This is because the
  // enums.xml values correspond to (a) special constants and (b) indexes of an
  // array containing resource IDs.
  base::UmaHistogramExactLinear(kUserImageChangedHistogramName, histogram_value,
                                default_user_image::kHistogramImagesCount + 1);
}

// static
void UserImageManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(UserImageManagerImpl::kUserImageProperties);
}

// Every image load or update is encapsulated by a Job. The Job is allowed to
// perform tasks on background threads or in helper processes but:
// * Changes to User objects and local state as well as any calls to the
//   `parent_` must be performed on the thread that the Job is created on only.
// * File writes and deletions must be performed via the `parent_`'s
//   `background_task_runner_` only.
//
// Only one of the Load*() and Set*() methods may be called per Job.
class UserImageManagerImpl::Job {
 public:
  // The `Job` will update the user object corresponding to `parent`.
  explicit Job(UserImageManagerImpl* parent);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job();

  // Loads the image at `image_path` or one of the default images,
  // depending on `image_index`, and updates the user object with the
  // new image.
  void LoadImage(base::FilePath image_path,
                 const int image_index,
                 const GURL& image_url);

  // Sets the user image in local state to the default image indicated
  // by `default_image_index`. Also updates the user object with the
  // new image.
  void SetToDefaultImage(int default_image_index);

  // Saves the `user_image` to disk and sets the user image in local
  // state to that image. Also updates the user with the new image.
  void SetToImage(int image_index,
                  std::unique_ptr<user_manager::UserImage> user_image);

  // Decodes the JPEG image `data`, crops and resizes the image, saves
  // it to disk and sets the user image in local state to that image.
  // Also updates the user object with the new image.
  void SetToImageData(std::unique_ptr<std::string> data);

  // Loads the image at `path`, transcodes it to JPEG format, saves
  // the image to disk and sets the user image in local state to that
  // image.  If `resize` is true, the image is cropped and resized
  // before transcoding.  Also updates the user object with the new
  // image.
  void SetToPath(const base::FilePath& path,
                 int image_index,
                 const GURL& image_url,
                 bool resize);

 private:
  // Called back after an image has been loaded from disk.
  void OnLoadImageDone(bool save,
                       std::unique_ptr<user_manager::UserImage> user_image);

  // Updates the user object with `user_image`.
  void UpdateUser(std::unique_ptr<user_manager::UserImage> user_image);

  // Updates the user object with `user_image`, and saves the image
  // bytes. Local state will be updated as needed.
  void UpdateUserAndSaveImage(
      std::unique_ptr<user_manager::UserImage> user_image);

  // Saves `image_bytes` to disk in `image_format` if
  // `image_is_safe_format`. Local state will be updated as needed.
  void SaveImageAndUpdateLocalState(
      bool image_is_safe_format,
      scoped_refptr<base::RefCountedBytes> image_bytes,
      user_manager::UserImage::ImageFormat image_format);

  // Called back after the user image has been saved to
  // disk. Updates the user image information in local state. The
  // information is only updated if `success` is true (indicating that
  // the image was saved successfully) or the user image is the
  // profile image (indicating that even if the image could not be
  // saved because it is not available right now, it will be
  // downloaded eventually).
  void OnSaveImageDone(bool success);

  // Updates the user image in local state, setting it to one of the
  // default images or the saved user image, depending on
  // `image_index_`.
  void UpdateLocalState();

  // Notifies the `parent_` that the Job is done.
  void NotifyJobDone();

  const AccountId& account_id() const { return parent_->account_id_; }

  UserImageLoaderDelegate* user_image_loader_delegate() {
    return parent_->user_image_loader_delegate_;
  }

  raw_ptr<UserImageManagerImpl, DanglingUntriaged> parent_;

  // Whether one of the Load*() or Set*() methods has been run already.
  bool run_;

  int image_index_;
  GURL image_url_;
  base::FilePath image_path_;
  bool image_cache_updated_ = false;

  base::WeakPtrFactory<Job> weak_factory_{this};
};

UserImageManagerImpl::Job::Job(UserImageManagerImpl* parent)
    : parent_(parent), run_(false) {}

UserImageManagerImpl::Job::~Job() {}

void UserImageManagerImpl::Job::LoadImage(base::FilePath image_path,
                                          const int image_index,
                                          const GURL& image_url) {
  DCHECK(!run_);
  run_ = true;

  image_index_ = image_index;
  image_url_ = image_url;
  image_path_ = image_path;

  if (default_user_image::IsValidIndex(image_index_)) {
    // Load one of the default images. This happens synchronously.
    if (const base::Value::Dict* image_properties =
            parent_->GetImageProperties()) {
      image_cache_updated_ =
          image_properties->FindBool(kImageCacheUpdated).value_or(false);
    }
    // Load default image from local cached version if available,
    // otherwise download from gstatic resources if possible.
    if (image_cache_updated_ && !image_path_.empty()) {
      user_image_loader::StartWithFilePathAnimated(
          parent_->background_task_runner_, image_path_,
          base::BindOnce(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(),
                         false));
    } else {
      if (g_skip_default_user_image_download) {
        auto user_image = std::make_unique<user_manager::UserImage>(
            *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                IDR_LOGIN_DEFAULT_USER));
        UpdateUser(std::move(user_image));
        UpdateLocalState();
        NotifyJobDone();
        return;
      }
      // Fetch the default image from cloud before caching it.
      image_url_ = default_user_image::GetDefaultImageUrl(image_index_);
      user_image_loader_delegate()->FromGURLAnimated(
          image_url_, base::BindOnce(&Job::OnLoadImageDone,
                                     weak_factory_.GetWeakPtr(), true));
    }
  } else if (image_index_ == user_manager::UserImage::Type::kExternal ||
             image_index_ == user_manager::UserImage::Type::kProfile) {
    // Load the user image from a file referenced by `image_path`. This happens
    // asynchronously. PNG_CODEC can be used here because LoadImage() is
    // called only for users whose user image has previously been set by one of
    // the Set*() methods, which transcode to JPEG or PNG format.
    DCHECK(!image_path_.empty());
    user_image_loader::StartWithFilePath(
        parent_->background_task_runner_, image_path_,
        ChooseCodecFromPath(image_path_),
        0,  // Do not crop.
        base::BindOnce(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(),
                       false));
  } else {
    NOTREACHED_IN_MIGRATION();
    NotifyJobDone();
  }
}

void UserImageManagerImpl::Job::SetToDefaultImage(int default_image_index) {
  DCHECK(!run_);
  run_ = true;

  DCHECK(default_user_image::IsValidIndex(default_image_index));

  image_index_ = default_image_index;

  // Fetch the default image from cloud before caching it.
  image_url_ = default_user_image::GetDefaultImageUrl(image_index_);

  // Set user image to a temp stub image while fetching the default image from
  // the cloud.
  auto user_image = std::make_unique<user_manager::UserImage>(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_LOGIN_DEFAULT_USER));
  UpdateUser(std::move(user_image));
  UpdateLocalState();

  if (g_skip_default_user_image_download) {
    NotifyJobDone();
    return;
  }

  user_image_loader_delegate()->FromGURLAnimated(
      image_url_,
      base::BindOnce(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(), true));
}

void UserImageManagerImpl::Job::SetToImage(
    int image_index,
    std::unique_ptr<user_manager::UserImage> user_image) {
  DCHECK(!run_);
  run_ = true;

  DCHECK(image_index == user_manager::UserImage::Type::kExternal ||
         image_index == user_manager::UserImage::Type::kProfile);

  image_index_ = image_index;

  UpdateUserAndSaveImage(std::move(user_image));
}

void UserImageManagerImpl::Job::SetToImageData(
    std::unique_ptr<std::string> data) {
  DCHECK(!run_);
  run_ = true;

  image_index_ = user_manager::UserImage::Type::kExternal;

  user_image_loader::StartWithData(
      parent_->background_task_runner_, std::move(data),
      ImageDecoder::DEFAULT_CODEC, login::kMaxUserImageSize,
      base::BindOnce(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(), true));
}

void UserImageManagerImpl::Job::SetToPath(const base::FilePath& path,
                                          int image_index,
                                          const GURL& image_url,
                                          bool resize) {
  DCHECK(!run_);
  run_ = true;

  image_index_ = image_index;
  image_url_ = image_url;

  DCHECK(!path.empty());
  user_image_loader::StartWithFilePath(
      parent_->background_task_runner_, path, ImageDecoder::DEFAULT_CODEC,
      resize ? login::kMaxUserImageSize : 0,
      base::BindOnce(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(), true));
}

void UserImageManagerImpl::Job::OnLoadImageDone(
    bool save,
    std::unique_ptr<user_manager::UserImage> user_image) {
  if (save) {
    UpdateUserAndSaveImage(std::move(user_image));
  } else {
    UpdateUser(std::move(user_image));
    NotifyJobDone();
  }
}

void UserImageManagerImpl::Job::UpdateUser(
    std::unique_ptr<user_manager::UserImage> user_image) {
  user_manager::User* user = parent_->GetUserAndModify();
  if (!user) {
    return;
  }
  if (!user_image->image().isNull()) {
    DCHECK(default_user_image::IsValidIndex(image_index_) ||
           user_image->has_image_bytes());
    user->SetImage(std::move(user_image), image_index_);
  } else {
    user->SetStubImage(
        std::make_unique<user_manager::UserImage>(
            *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                IDR_LOGIN_DEFAULT_USER)),
        image_index_, false);
  }
  user->SetImageURL(image_url_);

  parent_->OnJobChangedUserImage();
}

void UserImageManagerImpl::Job::UpdateUserAndSaveImage(
    std::unique_ptr<user_manager::UserImage> user_image) {
  const bool image_is_safe_format = user_image->is_safe_format();
  // Create a reference before user_image is passed.
  scoped_refptr<base::RefCountedBytes> image_bytes;
  if (image_is_safe_format) {
    image_bytes = user_image->image_bytes();
  }
  const user_manager::UserImage::ImageFormat image_format =
      user_image->image_format();

  UpdateUser(std::move(user_image));

  SaveImageAndUpdateLocalState(image_is_safe_format, image_bytes, image_format);
}

void UserImageManagerImpl::Job::SaveImageAndUpdateLocalState(
    bool image_is_safe_format,
    scoped_refptr<base::RefCountedBytes> image_bytes,
    user_manager::UserImage::ImageFormat image_format) {
  // Ignore if data stored or cached outside the user's cryptohome is to be
  // treated as ephemeral.
  if (parent_->user_manager_->IsUserNonCryptohomeDataEphemeral(account_id())) {
    OnSaveImageDone(false);
    return;
  }

  // This can happen if a stub profile image is chosen (i.e. the profile
  // image hasn't been downloaded yet).
  if (!image_bytes) {
    OnSaveImageDone(false);
    return;
  }

  // This should always be true, because of the following reasons:
  //
  // 1) Profile image from Google account -> UserImage is created with
  //    CreateAndEncode() that generates safe bytes representation.
  // 2) Profile image from user-specified image -> The bytes representation
  //    is regenerated after the original image is decoded and cropped.
  //
  // However, check the value just in case because an unsafe image should
  // never be saved.
  if (!image_is_safe_format) {
    LOG(ERROR) << "User image is not in safe format";
    OnSaveImageDone(false);
    return;
  }

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  // TODO(crbug.com/40496228): Use GetAccountIdKey() instead of GetUserEmail().
  image_path_ =
      user_data_dir.AppendASCII(account_id().GetUserEmail() +
                                ChooseExtensionFromImageFormat(image_format));

  // The old image file should be removed if the path is different. This
  // can happen if the user image format is changed from JPEG to PNG or
  // vice versa.
  base::FilePath old_image_path;
  // Because the user ID (i.e. email address) contains '.', the code here
  // cannot use the dots notation (path expantion) hence is verbose.
  if (const base::Value::Dict* image_properties =
          parent_->GetImageProperties()) {
    const std::string* value = image_properties->FindString(kImagePathNodeName);
    if (value) {
      old_image_path = base::FilePath::FromUTF8Unsafe(*value);
    }
  }

  parent_->background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveAndDeleteImage, image_bytes, image_path_,
                     old_image_path),
      base::BindOnce(&Job::OnSaveImageDone, weak_factory_.GetWeakPtr()));
}

void UserImageManagerImpl::Job::OnSaveImageDone(bool success) {
  image_cache_updated_ = success;
  if (success || image_index_ == user_manager::UserImage::Type::kProfile) {
    UpdateLocalState();
  }
  NotifyJobDone();
}

void UserImageManagerImpl::Job::UpdateLocalState() {
  // Ignore if data stored or cached outside the user's cryptohome is to be
  // treated as ephemeral.
  if (parent_->user_manager_->IsUserNonCryptohomeDataEphemeral(account_id())) {
    return;
  }

  PrefService* local_state = g_browser_process->local_state();

  base::Value::Dict entry;
  entry.Set(kImagePathNodeName, image_path_.value());
  entry.Set(kImageIndexNodeName, image_index_);
  entry.Set(kImageCacheUpdated, image_cache_updated_);
  if (!image_url_.is_empty()) {
    entry.Set(kImageURLNodeName, image_url_.spec());
  }

  const base::Value::Dict* existing_value =
      local_state->GetDict(kUserImageProperties)
          .FindDict(account_id().GetUserEmail());

  if (existing_value && *existing_value == entry) {
    return;
  }

  ScopedDictPrefUpdate update(local_state, kUserImageProperties);

  update->Set(account_id().GetUserEmail(), std::move(entry));

  parent_->user_manager_->NotifyLocalStateChanged();
}

void UserImageManagerImpl::Job::NotifyJobDone() {
  parent_->OnJobDone();
}

UserImageManagerImpl::UserImageManagerImpl(
    const AccountId& account_id,
    user_manager::UserManager* user_manager,
    UserImageLoaderDelegate* user_image_loader_delegate)
    : account_id_(account_id),
      user_manager_(user_manager),
      user_image_loader_delegate_(user_image_loader_delegate),
      downloading_profile_image_(false),
      has_managed_image_(false) {
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}

UserImageManagerImpl::~UserImageManagerImpl() {}

void UserImageManagerImpl::LoadUserImage() {
  // If the user image for `user_id` is managed by policy and the policy-set
  // image is being loaded and persisted right now, let that job continue. It
  // will update the user image when done.
  if (IsUserImageManaged() && job_.get()) {
    return;
  }

  const base::Value::Dict* image_properties = GetImageProperties();
  if (!image_properties) {
    SetInitialUserImage();
    return;
  }

  int image_index = image_properties->FindInt(kImageIndexNodeName)
                        .value_or(user_manager::UserImage::Type::kInvalid);
  if (image_index == user_manager::UserImage::Type::kInvalid) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  const std::string* image_url_string =
      image_properties->FindString(kImageURLNodeName);
  GURL image_url(image_url_string ? *image_url_string : std::string());
  const std::string* image_path =
      image_properties->FindString(kImagePathNodeName);

  user_manager::User* user = GetUserAndModify();
  user->SetImageURL(image_url);
  user->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      image_index, true);
  DCHECK((image_path && !image_path->empty()) ||
         image_index == user_manager::UserImage::Type::kProfile ||
         default_user_image::IsValidIndex(image_index));
  if (!default_user_image::IsValidIndex(image_index) &&
      (!image_path || image_path->empty())) {
    // Return if the profile image is to be used but has not been downloaded
    // yet. The profile image will be downloaded after login.
    return;
  }

  job_ = std::make_unique<Job>(this);
  job_->LoadImage(base::FilePath(*image_path), image_index, image_url);
}

void UserImageManagerImpl::UserLoggedIn(bool user_is_new, bool user_is_local) {
  // Reset the downloaded profile image as a new user logged in.
  downloaded_profile_image_ = gfx::ImageSkia();
  profile_image_url_ = GURL();

  is_random_image_set_ = false;
  const user_manager::User* user = GetUser();
  if (user_is_new) {
    if (!user_is_local) {
      SetInitialUserImage();
      is_random_image_set_ = true;
      // We should download the user image in this case, but at this moment the
      // user Profile instance is not yet ready. The actual downloading will be
      // handled in UserProfileCreated().
    }
  } else {
    // Although UserImage.LoggedIn3 is an enumerated histogram, we intentionally
    // use UmaHistogramExactLinear() to emit the metric rather than
    // UmaHistogramEnumeration(). This is because the enums.xml values
    // correspond to (a) special constants and (b) indexes of an array
    // containing resource IDs.
    base::UmaHistogramExactLinear(
        kUserImageLoggedInHistogramName,
        ImageIndexToHistogramIndex(user->image_index()),
        default_user_image::kHistogramImagesCount + 1);
  }

  user_image_sync_observer_.reset();
  TryToCreateImageSyncObserver();
}

void UserImageManagerImpl::UserProfileCreated() {
  if (IsUserLoggedInAndHasGaiaAccount()) {
    TryToInitDownloadedProfileImage();

    // Schedule an initial download of the profile data (full name and
    // optionally image).
    profile_download_one_shot_timer_.Start(
        FROM_HERE,
        g_ignore_profile_data_download_delay_
            ? base::TimeDelta()
            : base::Seconds(kProfileDataDownloadDelaySec),
        base::BindOnce(&UserImageManagerImpl::DownloadProfileData,
                       base::Unretained(this)));
    // Schedule periodic refreshes of the profile data.
    profile_download_periodic_timer_.Start(
        FROM_HERE, base::Seconds(kProfileRefreshIntervalSec),
        base::BindRepeating(&UserImageManagerImpl::DownloadProfileData,
                            base::Unretained(this)));
  } else {
    profile_download_one_shot_timer_.Stop();
    profile_download_periodic_timer_.Stop();
  }
}

void UserImageManagerImpl::SaveUserDefaultImageIndex(int default_image_index) {
  is_random_image_set_ = false;
  if (IsUserImageManaged()) {
    return;
  }
  job_ = std::make_unique<Job>(this);
  job_->SetToDefaultImage(default_image_index);
}

void UserImageManagerImpl::SaveUserImage(
    std::unique_ptr<user_manager::UserImage> user_image) {
  if (IsUserImageManaged() || !IsCustomizationSelectorsPrefEnabled()) {
    return;
  }
  job_ = std::make_unique<Job>(this);
  job_->SetToImage(user_manager::UserImage::Type::kExternal,
                   std::move(user_image));
}

void UserImageManagerImpl::SaveUserImageFromFile(const base::FilePath& path) {
  if (IsUserImageManaged() || !IsCustomizationSelectorsPrefEnabled()) {
    return;
  }
  job_ = std::make_unique<Job>(this);
  job_->SetToPath(path, user_manager::UserImage::Type::kExternal, GURL(), true);
}

void UserImageManagerImpl::SaveUserImageFromProfileImage() {
  if (IsUserImageManaged() || !IsCustomizationSelectorsPrefEnabled()) {
    return;
  }
  // Use the profile image if it has been downloaded already. Otherwise, use a
  // stub image (gray avatar).
  std::unique_ptr<user_manager::UserImage> user_image;
  if (downloaded_profile_image_.isNull()) {
    user_image = base::WrapUnique(new user_manager::UserImage);
  } else {
    user_image = user_manager::UserImage::CreateAndEncode(
        downloaded_profile_image_, user_manager::UserImage::ChooseImageFormat(
                                       *downloaded_profile_image_.bitmap()));
  }
  job_ = std::make_unique<Job>(this);
  job_->SetToImage(user_manager::UserImage::Type::kProfile,
                   std::move(user_image));
  // If no profile image has been downloaded yet, ensure that a download is
  // started.
  if (downloaded_profile_image_.isNull()) {
    DownloadProfileData();
  }
}

void UserImageManagerImpl::DeleteUserImage() {
  job_.reset();
  DeleteUserImageAndLocalStateEntry(kUserImageProperties);
}

const gfx::ImageSkia& UserImageManagerImpl::DownloadedProfileImage() const {
  return downloaded_profile_image_;
}

UserImageSyncObserver* UserImageManagerImpl::GetSyncObserver() const {
  return user_image_sync_observer_.get();
}

void UserImageManagerImpl::Shutdown() {
  profile_downloader_.reset();
  user_image_sync_observer_.reset();
}

bool UserImageManagerImpl::IsUserImageManaged() const {
  return has_managed_image_;
}

void UserImageManagerImpl::OnExternalDataSet(const std::string& policy) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  if (IsUserImageManaged()) {
    return;
  }

  has_managed_image_ = true;
  job_.reset();

  const user_manager::User* user = GetUser();
  if (!user) {
    return;
  }
  // If the user image for the currently logged-in user became managed, stop the
  // sync observer so that the policy-set image does not get synced out.
  if (user->is_logged_in()) {
    user_image_sync_observer_.reset();
  }

  user_manager_->NotifyUserImageIsEnterpriseManagedChanged(
      *user, /*is_enterprise_managed=*/true);
}

void UserImageManagerImpl::OnExternalDataCleared(const std::string& policy) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  if (!IsUserImageManaged()) {
    return;
  }

  has_managed_image_ = false;

  const auto* user = GetUser();
  if (user) {
    user_manager_->NotifyUserImageIsEnterpriseManagedChanged(
        *user, /*is_enterprise_managed=*/false);
  }
  SetInitialUserImage();
  TryToCreateImageSyncObserver();
}

void UserImageManagerImpl::OnExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  DCHECK(IsUserImageManaged());
  if (data) {
    job_ = std::make_unique<Job>(this);
    job_->SetToImageData(std::move(data));
  }
}

void UserImageManagerImpl::SetDownloadedProfileImageForTesting(
    const gfx::ImageSkia& image) {
  downloaded_profile_image_ = image;
}

// static
void UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting() {
  g_ignore_profile_data_download_delay_ = true;
}

// static
void UserImageManagerImpl::SkipProfileImageDownloadForTesting() {
  g_skip_profile_download = true;
}

// static
void UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting() {
  g_skip_default_user_image_download = true;
}

bool UserImageManagerImpl::NeedsProfilePicture() const {
  return downloading_profile_image_;
}

int UserImageManagerImpl::GetDesiredImageSideLength() const {
  return GetCurrentUserImageSize();
}

signin::IdentityManager* UserImageManagerImpl::GetIdentityManager() {
  const user_manager::User* user = GetUser();
  DCHECK(user && user->is_profile_created());
  return IdentityManagerFactory::GetForProfile(
      ProfileHelper::Get()->GetProfileByUser(user));
}

network::mojom::URLLoaderFactory* UserImageManagerImpl::GetURLLoaderFactory() {
  const user_manager::User* user = GetUser();
  DCHECK(user && user->is_profile_created());
  return ProfileHelper::Get()
      ->GetProfileByUser(user)
      ->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

std::string UserImageManagerImpl::GetCachedPictureURL() const {
  return profile_image_url_.spec();
}

void UserImageManagerImpl::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  // Ensure that the `profile_downloader_` is deleted when this method returns.
  std::unique_ptr<ProfileDownloader> profile_downloader(
      profile_downloader_.release());
  DCHECK_EQ(downloader, profile_downloader.get());

  user_manager_->UpdateUserAccountData(
      account_id_,
      user_manager::UserManager::UserAccountData(
          downloader->GetProfileFullName(), downloader->GetProfileGivenName(),
          downloader->GetProfileLocale()));
  if (!downloading_profile_image_) {
    return;
  }

  // Ignore the image if it is no longer needed.
  if (!NeedProfileImage()) {
    return;
  }

  const user_manager::User* const user = GetUser();

  if (downloader->GetProfilePictureStatus() ==
      ProfileDownloader::PICTURE_DEFAULT) {
    user_manager_->NotifyUserProfileImageUpdateFailed(*user);
  }

  // Nothing to do if the picture is cached or is the default avatar.
  if (downloader->GetProfilePictureStatus() !=
      ProfileDownloader::PICTURE_SUCCESS) {
    return;
  }

  downloaded_profile_image_ =
      gfx::ImageSkia::CreateFrom1xBitmap(downloader->GetProfilePicture());
  profile_image_url_ = GURL(downloader->GetProfilePictureURL());

  if (user->image_index() == user_manager::UserImage::Type::kProfile ||
      is_random_image_set_) {
    is_random_image_set_ = false;
    VLOG(1) << "Updating profile image for logged-in user.";
    // This will persist `downloaded_profile_image_` to disk.
    SaveUserImageFromProfileImage();
  }

  user_manager_->NotifyUserProfileImageUpdated(*user,
                                               downloaded_profile_image_);
}

void UserImageManagerImpl::OnProfileDownloadFailure(
    ProfileDownloader* downloader,
    ProfileDownloaderDelegate::FailureReason reason) {
  DCHECK_EQ(downloader, profile_downloader_.get());
  profile_downloader_.reset();

  if (reason == ProfileDownloaderDelegate::NETWORK_ERROR) {
    // Retry download after a delay if a network error occurred.
    profile_download_one_shot_timer_.Start(
        FROM_HERE, base::Seconds(kProfileDataDownloadRetryIntervalSec),
        base::BindOnce(&UserImageManagerImpl::DownloadProfileData,
                       base::Unretained(this)));
  }

  user_manager_->NotifyUserProfileImageUpdateFailed(*GetUser());
}

void UserImageManagerImpl::SetInitialUserImage() {
  // Choose a random default image.
  SaveUserDefaultImageIndex(default_user_image::GetRandomDefaultImageIndex());
}

void UserImageManagerImpl::TryToInitDownloadedProfileImage() {
  const user_manager::User* user = GetUser();
  if (user->image_index() == user_manager::UserImage::Type::kProfile &&
      downloaded_profile_image_.isNull() && !user->image_is_stub()) {
    // Initialize the `downloaded_profile_image_` for the currently logged-in
    // user if it has not been initialized already, the user image is the
    // profile image and the user image has been loaded successfully.
    VLOG(1) << "Profile image initialized from disk.";
    downloaded_profile_image_ = user->GetImage();
    profile_image_url_ = user->image_url();
  }
}

bool UserImageManagerImpl::NeedProfileImage() const {
  const user_manager::User* user = GetUser();
  return IsUserLoggedInAndHasGaiaAccount() &&
         IsCustomizationSelectorsPrefEnabled() &&
         user->image_index() == user_manager::UserImage::Type::kProfile;
}

void UserImageManagerImpl::DownloadProfileData() {
  if (!IsUserLoggedInAndHasGaiaAccount()) {
    return;
  }

  // If a download is already in progress, allow it to continue, with one
  // exception: If the current download does not include the profile image but
  // the image has since become necessary, start a new download that includes
  // the profile image.
  if (profile_downloader_ &&
      (downloading_profile_image_ || !NeedProfileImage())) {
    return;
  }

  downloading_profile_image_ = NeedProfileImage();
  profile_downloader_ = std::make_unique<ProfileDownloader>(this);
  profile_downloader_->Start();
}

void UserImageManagerImpl::DeleteUserImageAndLocalStateEntry(
    const char* prefs_dict_root) {
  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              prefs_dict_root);
  const base::Value::Dict* image_properties =
      update->FindDict(account_id_.GetUserEmail());
  if (!image_properties) {
    return;
  }

  const std::string* image_path =
      image_properties->FindString(kImagePathNodeName);
  if (image_path && !image_path->empty()) {
    background_task_runner_->PostTask(
        FROM_HERE, base::GetDeleteFileCallback(base::FilePath(*image_path)));
  }
  update->Remove(account_id_.GetUserEmail());
}

void UserImageManagerImpl::OnJobChangedUserImage() {
  if (GetUser()->is_logged_in()) {
    TryToInitDownloadedProfileImage();
  }

  user_manager_->NotifyUserImageChanged(*GetUser());
}

void UserImageManagerImpl::OnJobDone() {
  if (job_.get()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, job_.release());
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void UserImageManagerImpl::TryToCreateImageSyncObserver() {
  const user_manager::User* user = GetUser();
  // If the currently logged-in user's user image is managed, the sync observer
  // must not be started so that the policy-set image does not get synced out.
  // User image can be synced iff it has gaia account.
  if (!user_image_sync_observer_ && user && user->HasGaiaAccount() &&
      !IsUserImageManaged()) {
    user_image_sync_observer_ = std::make_unique<UserImageSyncObserver>(user);
  }
}

const base::Value::Dict* UserImageManagerImpl::GetImageProperties() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& prefs_images =
      local_state->GetDict(kUserImageProperties);

  const base::Value::Dict* image_properties =
      prefs_images.FindDict(account_id_.GetUserEmail());

  return image_properties;
}

const user_manager::User* UserImageManagerImpl::GetUser() const {
  return user_manager_->FindUser(account_id_);
}

user_manager::User* UserImageManagerImpl::GetUserAndModify() const {
  return user_manager_->FindUserAndModify(account_id_);
}

bool UserImageManagerImpl::IsUserLoggedInAndHasGaiaAccount() const {
  const user_manager::User* user = GetUser();
  if (!user) {
    return false;
  }
  return user->is_logged_in() && user->HasGaiaAccount();
}

bool UserImageManagerImpl::IsCustomizationSelectorsPrefEnabled() const {
  const user_manager::User* user = GetUser();
  // When this method is called, user Profile must be initialized already.
  auto* prefs = user->GetProfilePrefs();
  CHECK(prefs);
  return user_image::prefs::IsCustomizationSelectorsPrefEnabled(prefs);
}

}  // namespace ash

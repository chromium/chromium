// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

// Delay betweeen user login and attempt to update user's profile data.
const int kProfileDataDownloadDelaySec = 10;

// Interval betweeen retries to update user's profile data.
const int kProfileDataDownloadRetryIntervalSec = 300;

// Delay betweeen subsequent profile refresh attempts (24 hrs).
const int kProfileRefreshIntervalSec = 24 * 3600;

// Enum for reporting histograms about profile picture download.
enum ProfileDownloadResult {
  kDownloadSuccessChanged,
  kDownloadSuccess,
  kDownloadFailure,
  kDownloadDefault,
  kDownloadCached,

  // Must be the last, convenient count.
  kDownloadResultsCount
};

// Time histogram prefix for a cached profile image download.
const char kProfileDownloadCachedTime[] =
    "UserImage.ProfileDownloadTime.Cached";
// Time histogram prefix for the default profile image download.
const char kProfileDownloadDefaultTime[] =
    "UserImage.ProfileDownloadTime.Default";
// Time histogram prefix for a failed profile image download.
const char kProfileDownloadFailureTime[] =
    "UserImage.ProfileDownloadTime.Failure";
// Time histogram prefix for a successful profile image download.
const char kProfileDownloadSuccessTime[] =
    "UserImage.ProfileDownloadTime.Success";
// Time histogram suffix for a profile image download after login.
const char kProfileDownloadReasonLoggedIn[] = "LoggedIn";
// Time histogram suffix for a profile image download when the user chooses the
// profile image but it has not been downloaded yet.
const char kProfileDownloadReasonProfileImageChosen[] = "ProfileImageChosen";
// Time histogram suffix for a scheduled profile image download.
const char kProfileDownloadReasonScheduled[] = "Scheduled";
// Time histogram suffix for a profile image download retry.
const char kProfileDownloadReasonRetry[] = "Retry";

static bool g_ignore_profile_data_download_delay_ = false;

// Add a histogram showing the time it takes to download profile image.
// Separate histograms are reported for each download |reason| and |result|.
void AddProfileImageTimeHistogram(ProfileDownloadResult result,
                                  const std::string& download_reason,
                                  const base::TimeDelta& time_delta) {
  std::string histogram_name;
  switch (result) {
    case kDownloadFailure:
      histogram_name = kProfileDownloadFailureTime;
      break;
    case kDownloadDefault:
      histogram_name = kProfileDownloadDefaultTime;
      break;
    case kDownloadSuccess:
      histogram_name = kProfileDownloadSuccessTime;
      break;
    case kDownloadCached:
      histogram_name = kProfileDownloadCachedTime;
      break;
    default:
      NOTREACHED();
  }
  if (!download_reason.empty()) {
    histogram_name += ".";
    histogram_name += download_reason;
  }

  static const base::TimeDelta min_time = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta max_time = base::TimeDelta::FromSeconds(50);
  const size_t bucket_count(50);

  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram_name, min_time, max_time, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddTime(time_delta);

  DVLOG(1) << "Profile image download time: " << time_delta.InSecondsF();
}

// Converts |image_index| to UMA histogram value.
int ImageIndexToHistogramIndex(int image_index) {
  switch (image_index) {
    case user_manager::User::USER_IMAGE_EXTERNAL:
      // TODO(ivankr): Distinguish this from selected from file.
      return default_user_image::kHistogramImageFromCamera;
    case user_manager::User::USER_IMAGE_PROFILE:
      return default_user_image::kHistogramImageFromProfile;
    default:
      return image_index;
  }
}

// Saves |image_bytes| at |image_path|, and delete the old file at
// |old_image_path| if needed.
bool SaveAndDeleteImage(scoped_refptr<base::RefCountedBytes> image_bytes,
                        const base::FilePath& image_path,
                        const base::FilePath& old_image_path) {
  if (image_bytes->size() == 0 ||
      base::WriteFile(image_path,
                      reinterpret_cast<const char*>(image_bytes->front()),
                      image_bytes->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file: " << image_path.AsUTF8Unsafe();
    return false;
  }
  if (!old_image_path.empty() && old_image_path != image_path) {
    if (!base::DeleteFile(old_image_path, false /* recursive */)) {
      LOG(ERROR) << "Failed to delete old image: "
                 << old_image_path.AsUTF8Unsafe();
      return false;
    }
  }

  return true;
}

// Returns the robust codec enum for the given image path's extension.
// The user image is always stored in either JPEG or PNG.
ImageDecoder::ImageCodec ChooseRobustCodecFromPath(
    const base::FilePath& image_path) {
  if (image_path.Extension() == FILE_PATH_LITERAL(".jpg"))
    return ImageDecoder::ROBUST_JPEG_CODEC;
  if (image_path.Extension() == FILE_PATH_LITERAL(".png"))
    return ImageDecoder::ROBUST_PNG_CODEC;

  NOTREACHED() << "Invalid path: " << image_path.AsUTF8Unsafe();
  return ImageDecoder::ROBUST_JPEG_CODEC;
}

// Returns the suffix for the given image format, that should be JPEG or PNG.
const char* ChooseExtensionFromImageFormat(
    user_manager::UserImage::ImageFormat image_format) {
  switch (image_format) {
    case user_manager::UserImage::FORMAT_JPEG:
      return ".jpg";
    case user_manager::UserImage::FORMAT_PNG:
      return ".png";
    default:
      NOTREACHED() << "Invalid format: " << image_format;
      return ".jpg";
  }
}

}  // namespace

const char UserImageManagerImpl::kUserImageProperties[] = "user_image_info";
const char UserImageManagerImpl::kImagePathNodeName[] = "path";
const char UserImageManagerImpl::kImageIndexNodeName[] = "index";
const char UserImageManagerImpl::kImageURLNodeName[] = "url";

// static
void UserImageManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(UserImageManagerImpl::kUserImageProperties);
}

// Every image load or update is encapsulated by a Job. The Job is allowed to
// perform tasks on background threads or in helper processes but:
// * Changes to User objects and local state as well as any calls to the
//   |parent_| must be performed on the thread that the Job is created on only.
// * File writes and deletions must be performed via the |parent_|'s
//   |background_task_runner_| only.
//
// Only one of the Load*() and Set*() methods may be called per Job.
class UserImageManagerImpl::Job {
 public:
  // The |Job| will update the user object corresponding to |parent|.
  explicit Job(UserImageManagerImpl* parent);
  ~Job();

  // Loads the image at |image_path| or one of the default images,
  // depending on |image_index|, and updates the user object with the
  // new image.
  void LoadImage(base::FilePath image_path,
                 const int image_index,
                 const GURL& image_url);

  // Sets the user image in local state to the default image indicated
  // by |default_image_index|. Also updates the user object with the
  // new image.
  void SetToDefaultImage(int default_image_index);

  // Saves the |user_image| to disk and sets the user image in local
  // state to that image. Also updates the user with the new image.
  void SetToImage(int image_index,
                  std::unique_ptr<user_manager::UserImage> user_image);

  // Decodes the JPEG image |data|, crops and resizes the image, saves
  // it to disk and sets the user image in local state to that image.
  // Also updates the user object with the new image.
  void SetToImageData(std::unique_ptr<std::string> data);

  // Loads the image at |path|, transcodes it to JPEG format, saves
  // the image to disk and sets the user image in local state to that
  // image.  If |resize| is true, the image is cropped and resized
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

  // Updates the user object with |user_image|.
  void UpdateUser(std::unique_ptr<user_manager::UserImage> user_image);

  // Updates the user object with |user_image|, and saves the image
  // bytes. Local state will be updated as needed.
  void UpdateUserAndSaveImage(
      std::unique_ptr<user_manager::UserImage> user_image);

  // Saves |image_bytes| to disk in |image_format| if
  // |image_is_safe_format|. Local state will be updated as needed.
  void SaveImageAndUpdateLocalState(
      bool image_is_safe_format,
      scoped_refptr<base::RefCountedBytes> image_bytes,
      user_manager::UserImage::ImageFormat image_format);

  // Called back after the user image has been saved to
  // disk. Updates the user image information in local state. The
  // information is only updated if |success| is true (indicating that
  // the image was saved successfully) or the user image is the
  // profile image (indicating that even if the image could not be
  // saved because it is not available right now, it will be
  // downloaded eventually).
  void OnSaveImageDone(bool success);

  // Updates the user image in local state, setting it to one of the
  // default images or the saved user image, depending on
  // |image_index_|.
  void UpdateLocalState();

  // Notifies the |parent_| that the Job is done.
  void NotifyJobDone();

  const std::string& user_id() const { return parent_->user_id(); }

  UserImageManagerImpl* parent_;

  // Whether one of the Load*() or Set*() methods has been run already.
  bool run_;

  int image_index_;
  GURL image_url_;
  base::FilePath image_path_;

  base::WeakPtrFactory<Job> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Job);
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
    std::unique_ptr<user_manager::UserImage> user_image(
        new user_manager::UserImage(
            default_user_image::GetDefaultImage(image_index_)));
    UpdateUser(std::move(user_image));
    NotifyJobDone();
  } else if (image_index_ == user_manager::User::USER_IMAGE_EXTERNAL ||
             image_index_ == user_manager::User::USER_IMAGE_PROFILE) {
    // Load the user image from a file referenced by |image_path|. This happens
    // asynchronously. ROBUST_JPEG_CODEC or ROBUST_PNG_CODEC can be used here
    // because LoadImage() is called only for users whose user image has
    // previously been set by one of the Set*() methods, which transcode to
    // JPEG or PNG format.
    DCHECK(!image_path_.empty());
    user_image_loader::StartWithFilePath(
        parent_->background_task_runner_, image_path_,
        ChooseRobustCodecFromPath(image_path_),
        0,  // Do not crop.
        base::Bind(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(), false));
  } else {
    NOTREACHED();
    NotifyJobDone();
  }
}

void UserImageManagerImpl::Job::SetToDefaultImage(int default_image_index) {
  DCHECK(!run_);
  run_ = true;

  DCHECK(default_user_image::IsValidIndex(default_image_index));

  image_index_ = default_image_index;
  std::unique_ptr<user_manager::UserImage> user_image(
      new user_manager::UserImage(
          default_user_image::GetDefaultImage(image_index_)));

  UpdateUser(std::move(user_image));
  UpdateLocalState();
  NotifyJobDone();
}

void UserImageManagerImpl::Job::SetToImage(
    int image_index,
    std::unique_ptr<user_manager::UserImage> user_image) {
  DCHECK(!run_);
  run_ = true;

  DCHECK(image_index == user_manager::User::USER_IMAGE_EXTERNAL ||
         image_index == user_manager::User::USER_IMAGE_PROFILE);

  image_index_ = image_index;

  UpdateUserAndSaveImage(std::move(user_image));
}

void UserImageManagerImpl::Job::SetToImageData(
    std::unique_ptr<std::string> data) {
  DCHECK(!run_);
  run_ = true;

  image_index_ = user_manager::User::USER_IMAGE_EXTERNAL;

  // This method uses ROBUST_JPEG_CODEC, not DEFAULT_CODEC:
  // * This is necessary because the method is used to update the user image
  //   whenever the policy for a user is set. In the case of device-local
  //   accounts, policy may change at any time, even if the user is not
  //   currently logged in (and thus, DEFAULT_CODEC may not be used).
  // * This is possible because only JPEG |data| is accepted. No support for
  //   other image file formats is needed.
  // * This is safe because ROBUST_JPEG_CODEC employs a hardened JPEG decoder
  //   that protects against malicious invalid image data being used to attack
  //   the login screen or another user session currently in progress.
  user_image_loader::StartWithData(
      parent_->background_task_runner_, std::move(data),
      ImageDecoder::ROBUST_JPEG_CODEC, login::kMaxUserImageSize,
      base::Bind(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(), true));
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
      base::Bind(&Job::OnLoadImageDone, weak_factory_.GetWeakPtr(), true));
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
  if (!user)
    return;

  if (!user_image->image().isNull()) {
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
  if (image_is_safe_format)
    image_bytes = user_image->image_bytes();
  const user_manager::UserImage::ImageFormat image_format =
      user_image->image_format();

  UpdateUser(std::move(user_image));

  SaveImageAndUpdateLocalState(image_is_safe_format, image_bytes, image_format);
}

void UserImageManagerImpl::Job::SaveImageAndUpdateLocalState(
    bool image_is_safe_format,
    scoped_refptr<base::RefCountedBytes> image_bytes,
    user_manager::UserImage::ImageFormat image_format) {
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
  // 3) Profile image from policy (via OnExternalDataFetched()) -> JPEG is
  //    only allowed and ROBUST_JPEG_CODEC is used.
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
  // TODO(crbug.com/670557): Use GetAccountIdKey() instead of user_id().
  image_path_ = user_data_dir.AppendASCII(
      user_id() + ChooseExtensionFromImageFormat(image_format));

  // The old image file should be removed if the path is different. This
  // can happen if the user image format is changed from JPEG to PNG or
  // vice versa.
  base::FilePath old_image_path;
  // Because the user ID (i.e. email address) contains '.', the code here
  // cannot use the dots notation (path expantion) hence is verbose.
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* prefs_images =
      local_state->GetDictionary(kUserImageProperties);
  if (prefs_images) {
    const base::DictionaryValue* image_properties = nullptr;
    prefs_images->GetDictionaryWithoutPathExpansion(user_id(),
                                                    &image_properties);
    if (image_properties) {
      std::string value;
      image_properties->GetString(kImagePathNodeName, &value);
      old_image_path = base::FilePath::FromUTF8Unsafe(value);
    }
  }

  base::PostTaskAndReplyWithResult(
      parent_->background_task_runner_.get(), FROM_HERE,
      base::Bind(&SaveAndDeleteImage, image_bytes, image_path_, old_image_path),
      base::Bind(&Job::OnSaveImageDone, weak_factory_.GetWeakPtr()));
}

void UserImageManagerImpl::Job::OnSaveImageDone(bool success) {
  if (success || image_index_ == user_manager::User::USER_IMAGE_PROFILE)
    UpdateLocalState();
  NotifyJobDone();
}

void UserImageManagerImpl::Job::UpdateLocalState() {
  // Ignore if data stored or cached outside the user's cryptohome is to be
  // treated as ephemeral.
  if (parent_->user_manager_->IsUserNonCryptohomeDataEphemeral(
          AccountId::FromUserEmail(user_id())))
    return;

  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->Set(kImagePathNodeName,
             std::make_unique<base::Value>(image_path_.value()));
  entry->Set(kImageIndexNodeName, std::make_unique<base::Value>(image_index_));
  if (!image_url_.is_empty())
    entry->Set(kImageURLNodeName,
               std::make_unique<base::Value>(image_url_.spec()));
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              kUserImageProperties);
  update->SetWithoutPathExpansion(user_id(), std::move(entry));

  parent_->user_manager_->NotifyLocalStateChanged();
}

void UserImageManagerImpl::Job::NotifyJobDone() {
  parent_->OnJobDone();
}

UserImageManagerImpl::UserImageManagerImpl(
    const std::string& user_id,
    user_manager::UserManager* user_manager)
    : UserImageManager(user_id),
      user_manager_(user_manager),
      downloading_profile_image_(false),
      profile_image_requested_(false),
      has_managed_image_(false) {
  background_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}

UserImageManagerImpl::~UserImageManagerImpl() {}

void UserImageManagerImpl::LoadUserImage() {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* prefs_images =
      local_state->GetDictionary(kUserImageProperties);
  if (!prefs_images)
    return;
  user_manager::User* user = GetUserAndModify();

  const base::DictionaryValue* image_properties = nullptr;
  prefs_images->GetDictionaryWithoutPathExpansion(user_id(), &image_properties);

  // If the user image for |user_id| is managed by policy and the policy-set
  // image is being loaded and persisted right now, let that job continue. It
  // will update the user image when done.
  if (IsUserImageManaged() && job_.get())
    return;

  if (!image_properties) {
    SetInitialUserImage();
    return;
  }

  int image_index = user_manager::User::USER_IMAGE_INVALID;
  image_properties->GetInteger(kImageIndexNodeName, &image_index);
  if (default_user_image::IsValidIndex(image_index)) {
    user->SetImage(std::make_unique<user_manager::UserImage>(
                       default_user_image::GetDefaultImage(image_index)),
                   image_index);
    return;
  }

  if (image_index != user_manager::User::USER_IMAGE_EXTERNAL &&
      image_index != user_manager::User::USER_IMAGE_PROFILE) {
    NOTREACHED();
    return;
  }

  std::string image_url_string;
  image_properties->GetString(kImageURLNodeName, &image_url_string);
  GURL image_url(image_url_string);
  std::string image_path;
  image_properties->GetString(kImagePathNodeName, &image_path);

  user->SetImageURL(image_url);
  user->SetStubImage(
      std::make_unique<user_manager::UserImage>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER)),
      image_index, true);
  DCHECK(!image_path.empty() ||
         image_index == user_manager::User::USER_IMAGE_PROFILE);
  if (image_path.empty()) {
    // Return if the profile image is to be used but has not been downloaded
    // yet. The profile image will be downloaded after login.
    return;
  }

  job_.reset(new Job(this));
  job_->LoadImage(base::FilePath(image_path), image_index, image_url);
}

void UserImageManagerImpl::UserLoggedIn(bool user_is_new, bool user_is_local) {
  const user_manager::User* user = GetUser();
  if (user_is_new) {
    if (!user_is_local)
      SetInitialUserImage();
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR("UserImage.LoggedIn",
                               ImageIndexToHistogramIndex(user->image_index()),
                               default_user_image::kHistogramImagesCount);
  }

  // Reset the downloaded profile image as a new user logged in.
  downloaded_profile_image_ = gfx::ImageSkia();
  profile_image_url_ = GURL();
  profile_image_requested_ = false;

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
            : base::TimeDelta::FromSeconds(kProfileDataDownloadDelaySec),
        base::Bind(&UserImageManagerImpl::DownloadProfileData,
                   base::Unretained(this), kProfileDownloadReasonLoggedIn));
    // Schedule periodic refreshes of the profile data.
    profile_download_periodic_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kProfileRefreshIntervalSec),
        base::Bind(&UserImageManagerImpl::DownloadProfileData,
                   base::Unretained(this), kProfileDownloadReasonScheduled));
  } else {
    profile_download_one_shot_timer_.Stop();
    profile_download_periodic_timer_.Stop();
  }
}

void UserImageManagerImpl::SaveUserDefaultImageIndex(int default_image_index) {
  if (IsUserImageManaged())
    return;
  job_.reset(new Job(this));
  job_->SetToDefaultImage(default_image_index);
}

void UserImageManagerImpl::SaveUserImage(
    std::unique_ptr<user_manager::UserImage> user_image) {
  if (IsUserImageManaged())
    return;
  job_.reset(new Job(this));
  job_->SetToImage(user_manager::User::USER_IMAGE_EXTERNAL,
                   std::move(user_image));
}

void UserImageManagerImpl::SaveUserImageFromFile(const base::FilePath& path) {
  if (IsUserImageManaged())
    return;
  job_.reset(new Job(this));
  job_->SetToPath(path, user_manager::User::USER_IMAGE_EXTERNAL, GURL(), true);
}

void UserImageManagerImpl::SaveUserImageFromProfileImage() {
  if (IsUserImageManaged())
    return;
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
  job_.reset(new Job(this));
  job_->SetToImage(user_manager::User::USER_IMAGE_PROFILE,
                   std::move(user_image));
  // If no profile image has been downloaded yet, ensure that a download is
  // started.
  if (downloaded_profile_image_.isNull())
    DownloadProfileData(kProfileDownloadReasonProfileImageChosen);
}

void UserImageManagerImpl::DeleteUserImage() {
  job_.reset();
  DeleteUserImageAndLocalStateEntry(kUserImageProperties);
}

void UserImageManagerImpl::DownloadProfileImage(const std::string& reason) {
  profile_image_requested_ = true;
  DownloadProfileData(reason);
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

void UserImageManagerImpl::OnExternalDataSet(const std::string& policy) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  if (IsUserImageManaged())
    return;

  has_managed_image_ = true;
  job_.reset();

  const user_manager::User* user = GetUser();
  // If the user image for the currently logged-in user became managed, stop the
  // sync observer so that the policy-set image does not get synced out.
  if (user && user->is_logged_in())
    user_image_sync_observer_.reset();
}

void UserImageManagerImpl::OnExternalDataCleared(const std::string& policy) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  if (!IsUserImageManaged())
    return;

  has_managed_image_ = false;
  SetInitialUserImage();
  TryToCreateImageSyncObserver();
}

void UserImageManagerImpl::OnExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  DCHECK(IsUserImageManaged());
  if (data) {
    job_.reset(new Job(this));
    job_->SetToImageData(std::move(data));
  }
}

// static
void UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting() {
  g_ignore_profile_data_download_delay_ = true;
}

bool UserImageManagerImpl::NeedsProfilePicture() const {
  return downloading_profile_image_;
}

int UserImageManagerImpl::GetDesiredImageSideLength() const {
  return GetCurrentUserImageSize();
}

signin::IdentityManager* UserImageManagerImpl::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(
      ProfileHelper::Get()->GetProfileByUserUnsafe(GetUser()));
}

network::mojom::URLLoaderFactory* UserImageManagerImpl::GetURLLoaderFactory() {
  return content::BrowserContext::GetDefaultStoragePartition(
             ProfileHelper::Get()->GetProfileByUserUnsafe(GetUser()))
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

std::string UserImageManagerImpl::GetCachedPictureURL() const {
  return profile_image_url_.spec();
}

bool UserImageManagerImpl::IsPreSignin() const {
  return false;
}

void UserImageManagerImpl::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  // Ensure that the |profile_downloader_| is deleted when this method returns.
  std::unique_ptr<ProfileDownloader> profile_downloader(
      profile_downloader_.release());
  DCHECK_EQ(downloader, profile_downloader.get());

  user_manager_->UpdateUserAccountData(
      AccountId::FromUserEmail(user_id()),
      user_manager::UserManager::UserAccountData(
          downloader->GetProfileFullName(), downloader->GetProfileGivenName(),
          downloader->GetProfileLocale()));
  if (!downloading_profile_image_)
    return;

  ProfileDownloadResult result = kDownloadFailure;
  switch (downloader->GetProfilePictureStatus()) {
    case ProfileDownloader::PICTURE_SUCCESS:
      result = kDownloadSuccess;
      break;
    case ProfileDownloader::PICTURE_CACHED:
      result = kDownloadCached;
      break;
    case ProfileDownloader::PICTURE_DEFAULT:
      result = kDownloadDefault;
      break;
    default:
      NOTREACHED();
  }

  UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult", result,
                            kDownloadResultsCount);
  DCHECK(!profile_image_load_start_time_.is_null());
  AddProfileImageTimeHistogram(
      result, profile_image_download_reason_,
      base::TimeTicks::Now() - profile_image_load_start_time_);

  // Ignore the image if it is no longer needed.
  if (!NeedProfileImage())
    return;

  const user_manager::User* const user = GetUser();

  if (result == kDownloadDefault) {
    user_manager_->NotifyUserProfileImageUpdateFailed(*user);
  } else {
    profile_image_requested_ = false;
  }

  // Nothing to do if the picture is cached or is the default avatar.
  if (result != kDownloadSuccess)
    return;

  downloaded_profile_image_ =
      gfx::ImageSkia::CreateFrom1xBitmap(downloader->GetProfilePicture());
  profile_image_url_ = GURL(downloader->GetProfilePictureURL());

  if (user->image_index() == user_manager::User::USER_IMAGE_PROFILE) {
    VLOG(1) << "Updating profile image for logged-in user.";
    UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                              kDownloadSuccessChanged, kDownloadResultsCount);
    // This will persist |downloaded_profile_image_| to disk.
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

  if (downloading_profile_image_) {
    UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                              kDownloadFailure, kDownloadResultsCount);
    DCHECK(!profile_image_load_start_time_.is_null());
    AddProfileImageTimeHistogram(
        kDownloadFailure, profile_image_download_reason_,
        base::TimeTicks::Now() - profile_image_load_start_time_);
  }

  if (reason == ProfileDownloaderDelegate::NETWORK_ERROR) {
    // Retry download after a delay if a network error occurred.
    profile_download_one_shot_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kProfileDataDownloadRetryIntervalSec),
        base::Bind(&UserImageManagerImpl::DownloadProfileData,
                   base::Unretained(this), kProfileDownloadReasonRetry));
  }

  user_manager_->NotifyUserProfileImageUpdateFailed(*GetUser());
}

bool UserImageManagerImpl::IsUserImageManaged() const {
  return has_managed_image_;
}

void UserImageManagerImpl::SetInitialUserImage() {
  // Choose a random default image.
  SaveUserDefaultImageIndex(default_user_image::GetRandomDefaultImageIndex());
}

void UserImageManagerImpl::TryToInitDownloadedProfileImage() {
  const user_manager::User* user = GetUser();
  if (user->image_index() == user_manager::User::USER_IMAGE_PROFILE &&
      downloaded_profile_image_.isNull() && !user->image_is_stub()) {
    // Initialize the |downloaded_profile_image_| for the currently logged-in
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
         (user->image_index() == user_manager::User::USER_IMAGE_PROFILE ||
          profile_image_requested_);
}

void UserImageManagerImpl::DownloadProfileData(const std::string& reason) {
  if (!IsUserLoggedInAndHasGaiaAccount())
    return;

  // If a download is already in progress, allow it to continue, with one
  // exception: If the current download does not include the profile image but
  // the image has since become necessary, start a new download that includes
  // the profile image.
  if (profile_downloader_ &&
      (downloading_profile_image_ || !NeedProfileImage())) {
    return;
  }

  downloading_profile_image_ = NeedProfileImage();
  profile_image_download_reason_ = reason;
  profile_image_load_start_time_ = base::TimeTicks::Now();
  profile_downloader_.reset(new ProfileDownloader(this));
  profile_downloader_->Start();
}

void UserImageManagerImpl::DeleteUserImageAndLocalStateEntry(
    const char* prefs_dict_root) {
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs_dict_root);
  const base::DictionaryValue* image_properties;
  if (!update->GetDictionaryWithoutPathExpansion(user_id(), &image_properties))
    return;

  std::string image_path;
  image_properties->GetString(kImagePathNodeName, &image_path);
  if (!image_path.empty()) {
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                  base::FilePath(image_path), false));
  }
  update->RemoveWithoutPathExpansion(user_id(), nullptr);
}

void UserImageManagerImpl::OnJobChangedUserImage() {
  if (GetUser()->is_logged_in())
    TryToInitDownloadedProfileImage();

  user_manager_->NotifyUserImageChanged(*GetUser());
}

void UserImageManagerImpl::OnJobDone() {
  if (job_.get())
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, job_.release());
  else
    NOTREACHED();
}

void UserImageManagerImpl::TryToCreateImageSyncObserver() {
  const user_manager::User* user = GetUser();
  // If the currently logged-in user's user image is managed, the sync observer
  // must not be started so that the policy-set image does not get synced out.
  if (!user_image_sync_observer_ && user && user->CanSyncImage() &&
      !IsUserImageManaged()) {
    user_image_sync_observer_.reset(new UserImageSyncObserver(user));
  }
}

const user_manager::User* UserImageManagerImpl::GetUser() const {
  return user_manager_->FindUser(AccountId::FromUserEmail(user_id()));
}

user_manager::User* UserImageManagerImpl::GetUserAndModify() const {
  return user_manager_->FindUserAndModify(AccountId::FromUserEmail(user_id()));
}

bool UserImageManagerImpl::IsUserLoggedInAndHasGaiaAccount() const {
  const user_manager::User* user = GetUser();
  if (!user)
    return false;
  return user->is_logged_in() && user->HasGaiaAccount();
}

}  // namespace chromeos

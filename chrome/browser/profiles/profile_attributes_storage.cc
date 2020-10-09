// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_attributes_storage.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/string_compare.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/profile_metrics/state.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_list.h"
#endif

namespace {

using ImageData = std::vector<unsigned char>;

// First eight are generic icons, which use IDS_NUMBERED_PROFILE_NAME.
const int kDefaultNames[] = {
  IDS_DEFAULT_AVATAR_NAME_8,
  IDS_DEFAULT_AVATAR_NAME_9,
  IDS_DEFAULT_AVATAR_NAME_10,
  IDS_DEFAULT_AVATAR_NAME_11,
  IDS_DEFAULT_AVATAR_NAME_12,
  IDS_DEFAULT_AVATAR_NAME_13,
  IDS_DEFAULT_AVATAR_NAME_14,
  IDS_DEFAULT_AVATAR_NAME_15,
  IDS_DEFAULT_AVATAR_NAME_16,
  IDS_DEFAULT_AVATAR_NAME_17,
  IDS_DEFAULT_AVATAR_NAME_18,
  IDS_DEFAULT_AVATAR_NAME_19,
  IDS_DEFAULT_AVATAR_NAME_20,
  IDS_DEFAULT_AVATAR_NAME_21,
  IDS_DEFAULT_AVATAR_NAME_22,
  IDS_DEFAULT_AVATAR_NAME_23,
  IDS_DEFAULT_AVATAR_NAME_24,
  IDS_DEFAULT_AVATAR_NAME_25,
  IDS_DEFAULT_AVATAR_NAME_26
};

enum class MultiProfileUserType {
  kSingleProfile,       // There is only one profile.
  kActiveMultiProfile,  // Several profiles are actively used.
  kLatentMultiProfile   // There are several profiles, but only one is actively
                        // used.
};

// Reads a PNG from disk and decodes it. If the bitmap was successfully read
// from disk then this will return the bitmap image, otherwise it will return
// an empty gfx::Image.
gfx::Image ReadBitmap(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // If the path doesn't exist, don't even try reading it.
  if (!base::PathExists(image_path))
    return gfx::Image();

  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read PNG file from disk.";
    return gfx::Image();
  }

  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&image_data));
  if (image.IsEmpty())
    LOG(ERROR) << "Failed to decode PNG file.";

  return image;
}

// Writes |data| to disk and takes ownership of the pointer. On successful
// completion, it runs |callback|.
bool SaveBitmap(std::unique_ptr<ImageData> data,
                const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Make sure the destination directory exists.
  base::FilePath dir = image_path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create parent directory.";
    return false;
  }

  if (base::WriteFile(image_path, reinterpret_cast<char*>(&(*data)[0]),
                      data->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file.";
    return false;
  }
  return true;
}

void RunCallbackIfFileMissing(const base::FilePath& file_path,
                              const base::Closure& callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(file_path))
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, callback);
}

// Compares two ProfileAttributesEntry using locale-sensitive comparison of
// their names. For ties, the profile path is compared next.
class ProfileAttributesSortComparator {
 public:
  explicit ProfileAttributesSortComparator(icu::Collator* collator);
  bool operator()(const ProfileAttributesEntry* const a,
                  const ProfileAttributesEntry* const b) const;
 private:
  icu::Collator* collator_;
};

ProfileAttributesSortComparator::ProfileAttributesSortComparator(
    icu::Collator* collator) : collator_(collator) {}

bool ProfileAttributesSortComparator::operator()(
    const ProfileAttributesEntry* const a,
    const ProfileAttributesEntry* const b) const {
  UCollationResult result = base::i18n::CompareString16WithCollator(
      *collator_, a->GetName(), b->GetName());
  if (result != UCOL_EQUAL)
    return result == UCOL_LESS;

  // If the names are the same, then compare the paths, which must be unique.
  return a->GetPath().value() < b->GetPath().value();
}

MultiProfileUserType GetMultiProfileUserType(
    const std::vector<ProfileAttributesEntry*>& entries) {
  DCHECK_GT(entries.size(), 0u);
  if (entries.size() == 1u)
    return MultiProfileUserType::kSingleProfile;

  int active_count = std::count_if(
      entries.begin(), entries.end(), [](ProfileAttributesEntry* entry) {
        return ProfileMetrics::IsProfileActive(entry);
      });

  if (active_count <= 1)
    return MultiProfileUserType::kLatentMultiProfile;
  return MultiProfileUserType::kActiveMultiProfile;
}

profile_metrics::AvatarState GetAvatarState(ProfileAttributesEntry* entry) {
  size_t index = entry->GetAvatarIconIndex();
  bool is_modern = profiles::IsModernAvatarIconIndex(index);
  if (entry->GetSigninState() == SigninState::kNotSignedIn) {
    if (index == profiles::GetPlaceholderAvatarIndex())
      return profile_metrics::AvatarState::kSignedOutDefault;
    return is_modern ? profile_metrics::AvatarState::kSignedOutModern
                     : profile_metrics::AvatarState::kSignedOutOld;
  }
  if (entry->IsUsingGAIAPicture())
    return profile_metrics::AvatarState::kSignedInGaia;
  return is_modern ? profile_metrics::AvatarState::kSignedInModern
                   : profile_metrics::AvatarState::kSignedInOld;
}

profile_metrics::NameState GetNameState(ProfileAttributesEntry* entry) {
  bool has_default_name = entry->IsUsingDefaultName();
  switch (entry->GetNameForm()) {
    case NameForm::kGaiaName:
      return profile_metrics::NameState::kGaiaName;
    case NameForm::kLocalName:
      return has_default_name ? profile_metrics::NameState::kDefaultName
                              : profile_metrics::NameState::kCustomName;
    case NameForm::kGaiaAndLocalName:
      return has_default_name ? profile_metrics::NameState::kGaiaAndDefaultName
                              : profile_metrics::NameState::kGaiaAndCustomName;
  }
}

profile_metrics::UnconsentedPrimaryAccountType GetUnconsentedPrimaryAccountType(
    ProfileAttributesEntry* entry) {
  if (entry->GetSigninState() == SigninState::kNotSignedIn)
    return profile_metrics::UnconsentedPrimaryAccountType::kSignedOut;
  if (entry->IsChild())
    return profile_metrics::UnconsentedPrimaryAccountType::kChild;
  // TODO(crbug.com/1060113): Replace this check by
  // !entry->GetHostedDomain().has_value() in M84 (once the cache gets
  // reasonably well populated).
  if (policy::BrowserPolicyConnector::IsNonEnterpriseUser(
          base::UTF16ToUTF8(entry->GetUserName()))) {
    return profile_metrics::UnconsentedPrimaryAccountType::kConsumer;
  }
  // TODO(crbug.com/1060113): Figure out how to distinguish EDU accounts from
  // other enterprise.
  return profile_metrics::UnconsentedPrimaryAccountType::kEnterprise;
}

void RecordProfileState(ProfileAttributesEntry* entry,
                        profile_metrics::StateSuffix suffix) {
  profile_metrics::LogProfileAvatar(GetAvatarState(entry), suffix);
  profile_metrics::LogProfileName(GetNameState(entry), suffix);
  profile_metrics::LogProfileAccountType(
      GetUnconsentedPrimaryAccountType(entry), suffix);
  profile_metrics::LogProfileSyncEnabled(
      entry->GetSigninState() ==
          SigninState::kSignedInWithConsentedPrimaryAccount,
      suffix);
  profile_metrics::LogProfileDaysSinceLastUse(
      (base::Time::Now() - entry->GetActiveTime()).InDays(), suffix);
}

}  // namespace

ProfileAttributesStorage::ProfileAttributesStorage(PrefService* prefs)
    : prefs_(prefs),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

ProfileAttributesStorage::~ProfileAttributesStorage() {
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributes() {
  std::vector<ProfileAttributesEntry*> ret;
  for (const auto& path_and_entry : profile_attributes_entries_) {
    ProfileAttributesEntry* entry;
    // Initialize any entries that are not yet initialized.
    bool success = GetProfileAttributesWithPath(
        base::FilePath(path_and_entry.first), &entry);
    DCHECK(success);
    ret.push_back(entry);
  }
  return ret;
}

std::vector<ProfileAttributesEntry*>
ProfileAttributesStorage::GetAllProfilesAttributesSortedByName() {
  std::vector<ProfileAttributesEntry*> ret = GetAllProfilesAttributes();
  // Do not allocate the collator and sort if it is not necessary.
  if (ret.size() < 2)
    return ret;

  UErrorCode error_code = U_ZERO_ERROR;
  // Use the default collator. The default locale should have been properly
  // set by the time this constructor is called.
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));
  DCHECK(U_SUCCESS(error_code));

  std::sort(ret.begin(), ret.end(),
            ProfileAttributesSortComparator(collator.get()));
  return ret;
}

base::string16 ProfileAttributesStorage::ChooseNameForNewProfile(
    size_t icon_index) const {
  base::string16 name;
  for (int name_index = 1; ; ++name_index) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    // Using native digits will break IsDefaultProfileName() below because
    // it uses sscanf.
    // TODO(jshin): fix IsDefaultProfileName to handle native digits.
    name = l10n_util::GetStringFUTF16(IDS_NEW_NUMBERED_PROFILE_NAME,
                                      base::NumberToString16(name_index));
#else
    // TODO(crbug.com/937834): Clean up this code.
    if (icon_index < profiles::GetGenericAvatarIconCount() ||
        profiles::IsModernAvatarIconIndex(icon_index)) {
      name = l10n_util::GetStringFUTF16Int(IDS_NUMBERED_PROFILE_NAME,
                                           name_index);
    } else {
      // TODO(jshin): Check with UX if appending |name_index| to the default
      // name without a space is intended.
      name = l10n_util::GetStringUTF16(
          kDefaultNames[icon_index - profiles::GetGenericAvatarIconCount()]);
      if (name_index > 1)
        name.append(base::FormatNumber(name_index));
    }
#endif

    // Loop through previously named profiles to ensure we're not duplicating.
    std::vector<ProfileAttributesEntry*> entries =
        const_cast<ProfileAttributesStorage*>(this)->GetAllProfilesAttributes();

    if (std::none_of(entries.begin(), entries.end(),
                     [name](ProfileAttributesEntry* entry) {
                       return entry->GetLocalProfileName() == name ||
                              entry->GetName() == name;
                     })) {
      return name;
    }
  }
}

bool ProfileAttributesStorage::IsDefaultProfileName(
    const base::string16& name,
    bool include_check_for_legacy_profile_name) const {
  // Check whether it's one of the "Person %d" style names.
  std::string default_name_format = l10n_util::GetStringFUTF8(
      IDS_NEW_NUMBERED_PROFILE_NAME, base::ASCIIToUTF16("%d"));
  int generic_profile_number;  // Unused. Just a placeholder for sscanf.
  int assignments =
      sscanf(base::UTF16ToUTF8(name).c_str(), default_name_format.c_str(),
             &generic_profile_number);
  if (assignments == 1)
    return true;

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  if (!include_check_for_legacy_profile_name)
    return false;
#endif

  // Check if it's a "First user" old-style name.
  if (name == l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME) ||
      name == l10n_util::GetStringUTF16(IDS_LEGACY_DEFAULT_PROFILE_NAME))
    return true;

  // Check if it's one of the old-style profile names.
  for (size_t i = 0; i < base::size(kDefaultNames); ++i) {
    if (name == l10n_util::GetStringUTF16(kDefaultNames[i]))
      return true;
  }
  return false;
}

size_t ProfileAttributesStorage::ChooseAvatarIconIndexForNewProfile() const {
  std::unordered_set<size_t> used_icon_indices;

  std::vector<ProfileAttributesEntry*> entries =
      const_cast<ProfileAttributesStorage*>(this)->GetAllProfilesAttributes();
  for (const ProfileAttributesEntry* entry : entries)
    used_icon_indices.insert(entry->GetAvatarIconIndex());

  return profiles::GetRandomAvatarIconIndex(used_icon_indices);
}

const gfx::Image* ProfileAttributesStorage::LoadAvatarPictureFromPath(
    const base::FilePath& profile_path,
    const std::string& key,
    const base::FilePath& image_path) const {
  // If the picture is already loaded then use it.
  if (cached_avatar_images_.count(key)) {
    if (cached_avatar_images_[key].IsEmpty())
      return nullptr;
    return &cached_avatar_images_[key];
  }

  // Don't download the image if downloading is disabled for tests.
  if (disable_avatar_download_for_testing_)
    return nullptr;

  // If the picture is already being loaded then don't try loading it again.
  if (cached_avatar_images_loading_[key])
    return nullptr;
  cached_avatar_images_loading_[key] = true;

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ReadBitmap, image_path),
      base::BindOnce(&ProfileAttributesStorage::OnAvatarPictureLoaded,
                     const_cast<ProfileAttributesStorage*>(this)->AsWeakPtr(),
                     profile_path, key));
  return nullptr;
}

void ProfileAttributesStorage::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void ProfileAttributesStorage::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

#if !defined(OS_ANDROID)
void ProfileAttributesStorage::RecordDeletedProfileState(
    ProfileAttributesEntry* entry) {
  DCHECK(entry);
  RecordProfileState(entry, profile_metrics::StateSuffix::kUponDeletion);
  bool is_last_profile = GetNumberOfProfiles() <= 1u;
  // If the profile has windows opened, they are still open at this moment.
  // Thus, this really means that only the profile manager is open.
  bool no_browser_windows = BrowserList::GetInstance()->empty();
  profile_metrics::LogProfileDeletionContext(is_last_profile,
                                             no_browser_windows);
}
#endif

void ProfileAttributesStorage::RecordProfilesState() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  if (entries.size() == 0)
    return;

  MultiProfileUserType type = GetMultiProfileUserType(entries);

  for (ProfileAttributesEntry* entry : entries) {
    RecordProfileState(entry, profile_metrics::StateSuffix::kAll);

    switch (type) {
      case MultiProfileUserType::kSingleProfile:
        RecordProfileState(entry, profile_metrics::StateSuffix::kSingleProfile);
        break;
      case MultiProfileUserType::kActiveMultiProfile:
        RecordProfileState(entry,
                           profile_metrics::StateSuffix::kActiveMultiProfile);
        break;
      case MultiProfileUserType::kLatentMultiProfile: {
        RecordProfileState(entry,
                           profile_metrics::StateSuffix::kLatentMultiProfile);
        if (ProfileMetrics::IsProfileActive(entry)) {
          RecordProfileState(
              entry, profile_metrics::StateSuffix::kLatentMultiProfileActive);
        } else {
          RecordProfileState(
              entry, profile_metrics::StateSuffix::kLatentMultiProfileOthers);
        }
        break;
      }
    }
  }
}

void ProfileAttributesStorage::NotifyOnProfileAvatarChanged(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(profile_path);
}

void ProfileAttributesStorage::NotifyOnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) const {
  for (auto& observer : observer_list_)
    observer.OnProfileHighResAvatarLoaded(profile_path);
}

void ProfileAttributesStorage::DownloadHighResAvatarIfNeeded(
    size_t icon_index,
    const base::FilePath& profile_path) {
#if defined(OS_ANDROID)
  return;
#endif
  DCHECK(!disable_avatar_download_for_testing_);

  // If this is the placeholder avatar, it is already included in the
  // resources, so it doesn't need to be downloaded (and it will never be
  // requested from disk by GetHighResAvatarOfProfileAtIndex).
  if (icon_index == profiles::GetPlaceholderAvatarIndex())
    return;

  const base::FilePath& file_path =
      profiles::GetPathOfHighResAvatarAtIndex(icon_index);
  base::Closure callback =
      base::Bind(&ProfileAttributesStorage::DownloadHighResAvatar, AsWeakPtr(),
                 icon_index, profile_path);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RunCallbackIfFileMissing, file_path, callback));
}

void ProfileAttributesStorage::DownloadHighResAvatar(
    size_t icon_index,
    const base::FilePath& profile_path) {
#if !defined(OS_ANDROID)
  const char* file_name =
      profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index);
  DCHECK(file_name);
  // If the file is already being downloaded, don't start another download.
  if (avatar_images_downloads_in_progress_.count(file_name))
    return;

  // Start the download for this file. The profile attributes storage takes
  // ownership of the avatar downloader, which will be deleted when the download
  // completes, or if that never happens, when the storage is destroyed.
  std::unique_ptr<ProfileAvatarDownloader>& current_downloader =
      avatar_images_downloads_in_progress_[file_name];
  current_downloader.reset(new ProfileAvatarDownloader(
      icon_index,
      base::BindOnce(&ProfileAttributesStorage::SaveAvatarImageAtPathNoCallback,
                     AsWeakPtr(), profile_path)));

  current_downloader->Start();
#endif
}

void ProfileAttributesStorage::SaveAvatarImageAtPath(
    const base::FilePath& profile_path,
    gfx::Image image,
    const std::string& key,
    const base::FilePath& image_path,
    base::OnceClosure callback) {
  cached_avatar_images_[key] = image;

  std::unique_ptr<ImageData> data(new ImageData);
  scoped_refptr<base::RefCountedMemory> png_data = image.As1xPNGBytes();
  data->assign(png_data->front(), png_data->front() + png_data->size());

  // Remove the file from the list of downloads in progress. Note that this list
  // only contains the high resolution avatars, and not the Gaia profile images.
  auto downloader_iter = avatar_images_downloads_in_progress_.find(key);
  if (downloader_iter != avatar_images_downloads_in_progress_.end()) {
    // We mustn't delete the avatar downloader right here, since we're being
    // called by it.
    content::GetUIThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, downloader_iter->second.release());
    avatar_images_downloads_in_progress_.erase(downloader_iter);
  }

  if (data->empty()) {
    LOG(ERROR) << "Failed to PNG encode the image.";
  } else {
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(), FROM_HERE,
        base::BindOnce(&SaveBitmap, std::move(data), image_path),
        base::BindOnce(&ProfileAttributesStorage::OnAvatarPictureSaved,
                       AsWeakPtr(), key, profile_path, std::move(callback)));
  }
}

void ProfileAttributesStorage::OnAvatarPictureLoaded(
    const base::FilePath& profile_path,
    const std::string& key,
    gfx::Image image) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cached_avatar_images_loading_[key] = false;
  if (cached_avatar_images_.count(key)) {
    if (!cached_avatar_images_[key].IsEmpty() || image.IsEmpty()) {
      // If GAIA picture is not empty that means that it has been set with the
      // most up-to-date value while the picture was being loaded from disk.
      // If GAIA picture is empty and the image loaded from disk is also empty
      // then there is no need to update.
      return;
    }
  }

  // Even if the image is empty (e.g. because decoding failed), place it in the
  // cache to avoid reloading it again.
  cached_avatar_images_[key] = std::move(image);

  NotifyOnProfileHighResAvatarLoaded(profile_path);
}

void ProfileAttributesStorage::OnAvatarPictureSaved(
    const std::string& file_name,
    const base::FilePath& profile_path,
    base::OnceClosure callback,
    bool success) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    return;

  if (callback)
    std::move(callback).Run();

  NotifyOnProfileHighResAvatarLoaded(profile_path);
}

void ProfileAttributesStorage::SaveAvatarImageAtPathNoCallback(
    const base::FilePath& profile_path,
    gfx::Image image,
    const std::string& key,
    const base::FilePath& image_path) {
  SaveAvatarImageAtPath(profile_path, image, key, image_path,
                        base::OnceClosure());
}

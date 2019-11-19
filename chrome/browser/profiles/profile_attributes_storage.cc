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
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

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
void SaveBitmap(std::unique_ptr<ImageData> data,
                const base::FilePath& image_path,
                const base::Closure& callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Make sure the destination directory exists.
  base::FilePath dir = image_path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create parent directory.";
    return;
  }

  if (base::WriteFile(image_path, reinterpret_cast<char*>(&(*data)[0]),
                      data->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file.";
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, callback);
}

void RunCallbackIfFileMissing(const base::FilePath& file_path,
                              const base::Closure& callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(file_path))
    base::PostTask(FROM_HERE, {content::BrowserThread::UI}, callback);
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

}  // namespace

ProfileAttributesStorage::ProfileAttributesStorage(PrefService* prefs)
    : prefs_(prefs),
      file_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE,
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
// Downloading is only supported on desktop.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
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
// Downloading is only supported on desktop.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  return;
#endif
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
      icon_index, base::Bind(&ProfileAttributesStorage::SaveAvatarImageAtPath,
                             AsWeakPtr(), profile_path)));

  current_downloader->Start();
}

void ProfileAttributesStorage::SaveAvatarImageAtPath(
    const base::FilePath& profile_path,
    gfx::Image image,
    const std::string& key,
    const base::FilePath& image_path) {
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
    base::DeleteSoon(FROM_HERE, {content::BrowserThread::UI},
                     downloader_iter->second.release());
    avatar_images_downloads_in_progress_.erase(downloader_iter);
  }

  if (data->empty()) {
    LOG(ERROR) << "Failed to PNG encode the image.";
  } else {
    base::Closure callback =
        base::Bind(&ProfileAttributesStorage::OnAvatarPictureSaved, AsWeakPtr(),
                   key, profile_path);
    file_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SaveBitmap, std::move(data), image_path, callback));
  }
}

void ProfileAttributesStorage::OnAvatarPictureLoaded(
    const base::FilePath& profile_path,
    const std::string& key,
    gfx::Image image) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cached_avatar_images_loading_[key] = false;

  // Even if the image is empty (e.g. because decoding failed), place it in the
  // cache to avoid reloading it again.
  cached_avatar_images_[key] = std::move(image);

  NotifyOnProfileHighResAvatarLoaded(profile_path);
}

void ProfileAttributesStorage::OnAvatarPictureSaved(
    const std::string& file_name,
    const base::FilePath& profile_path) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NotifyOnProfileHighResAvatarLoaded(profile_path);
}

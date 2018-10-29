// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

namespace {

const char kNameKey[] = "name";
const char kGAIANameKey[] = "gaia_name";
const char kGAIAGivenNameKey[] = "gaia_given_name";
const char kGAIAIdKey[] = "gaia_id";
const char kIsUsingDefaultNameKey[] = "is_using_default_name";
const char kIsUsingDefaultAvatarKey[] = "is_using_default_avatar";
const char kUseGAIAPictureKey[] = "use_gaia_picture";
const char kGAIAPictureFileNameKey[] = "gaia_picture_file_name";
const char kIsOmittedFromProfileListKey[] = "is_omitted_from_profile_list";
const char kSigninRequiredKey[] = "signin_required";
const char kSupervisedUserId[] = "managed_user_id";
const char kAccountIdKey[] = "account_id_key";

// TODO(dullweber): Remove these constants after the stored data is removed.
const char kStatsBrowsingHistoryKeyDeprecated[] = "stats_browsing_history";
const char kStatsPasswordsKeyDeprecated[] = "stats_passwords";
const char kStatsBookmarksKeyDeprecated[] = "stats_bookmarks";
const char kStatsSettingsKeyDeprecated[] = "stats_settings";

void DeleteBitmap(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  base::DeleteFile(image_path, false);
}

}  // namespace

ProfileInfoCache::ProfileInfoCache(PrefService* prefs,
                                   const base::FilePath& user_data_dir)
    : ProfileAttributesStorage(prefs), user_data_dir_(user_data_dir) {
  // Populate the cache
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  for (base::DictionaryValue::Iterator it(*cache);
       !it.IsAtEnd(); it.Advance()) {
    base::DictionaryValue* info = NULL;
    cache->GetDictionaryWithoutPathExpansion(it.key(), &info);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && !defined(OS_ANDROID) && \
    !defined(OS_CHROMEOS)
    std::string supervised_user_id;
    info->GetString(kSupervisedUserId, &supervised_user_id);
    // Silently ignore legacy supervised user profiles.
    if (!supervised_user_id.empty() &&
        supervised_user_id != supervised_users::kChildAccountSUID) {
      continue;
    }
#endif
    base::string16 name;
    info->GetString(kNameKey, &name);
    sorted_keys_.insert(FindPositionForProfile(it.key(), name), it.key());
    profile_attributes_entries_[user_data_dir_.AppendASCII(it.key()).value()] =
        std::unique_ptr<ProfileAttributesEntry>(nullptr);

    bool using_default_name;
    if (!info->GetBoolean(kIsUsingDefaultNameKey, &using_default_name)) {
      // If the preference hasn't been set, and the name is default, assume
      // that the user hasn't done this on purpose.
      using_default_name = IsDefaultProfileName(name);
      info->SetBoolean(kIsUsingDefaultNameKey, using_default_name);
    }

    // For profiles that don't have the "using default avatar" state set yet,
    // assume it's the same as the "using default name" state.
    if (!info->HasKey(kIsUsingDefaultAvatarKey)) {
      info->SetBoolean(kIsUsingDefaultAvatarKey, using_default_name);
    }
  }

  // If needed, start downloading the high-res avatars and migrate any legacy
  // profile names.
  if (!disable_avatar_download_for_testing_)
    MigrateLegacyProfileNamesAndDownloadAvatars();

  RemoveDeprecatedStatistics();
}

ProfileInfoCache::~ProfileInfoCache() {
}

void ProfileInfoCache::AddProfileToCache(const base::FilePath& profile_path,
                                         const base::string16& name,
                                         const std::string& gaia_id,
                                         const base::string16& user_name,
                                         size_t icon_index,
                                         const std::string& supervised_user_id,
                                         const AccountId& account_id) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && !defined(OS_ANDROID) && \
    !defined(OS_CHROMEOS)
  // Silently ignore legacy supervised user profiles.
  if (!supervised_user_id.empty() &&
      supervised_user_id != supervised_users::kChildAccountSUID) {
    return;
  }
#endif
  std::string key = CacheKeyFromProfilePath(profile_path);
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();

  std::unique_ptr<base::DictionaryValue> info(new base::DictionaryValue);
  info->SetString(kNameKey, name);
  info->SetString(kGAIAIdKey, gaia_id);
  info->SetString(ProfileAttributesEntry::kUserNameKey, user_name);
  info->SetString(ProfileAttributesEntry::kAvatarIconKey,
                  profiles::GetDefaultAvatarIconUrl(icon_index));
  // Default value for whether background apps are running is false.
  info->SetBoolean(ProfileAttributesEntry::kBackgroundAppsKey, false);
  info->SetString(kSupervisedUserId, supervised_user_id);
  info->SetBoolean(kIsOmittedFromProfileListKey, !supervised_user_id.empty());
  info->SetBoolean(ProfileAttributesEntry::kProfileIsEphemeral, false);
  info->SetBoolean(kIsUsingDefaultNameKey, IsDefaultProfileName(name));
  // Assume newly created profiles use a default avatar.
  info->SetBoolean(kIsUsingDefaultAvatarKey, true);
  if (account_id.HasAccountIdKey())
    info->SetString(kAccountIdKey, account_id.GetAccountIdKey());
  cache->SetWithoutPathExpansion(key, std::move(info));

  sorted_keys_.insert(FindPositionForProfile(key, name), key);
  profile_attributes_entries_[user_data_dir_.AppendASCII(key).value()] =
      std::unique_ptr<ProfileAttributesEntry>();

  if (!disable_avatar_download_for_testing_)
    DownloadHighResAvatarIfNeeded(icon_index, profile_path);

  for (auto& observer : observer_list_)
    observer.OnProfileAdded(profile_path);
}

void ProfileInfoCache::DeleteProfileFromCache(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry;
  if (!GetProfileAttributesWithPath(profile_path, &entry)) {
    NOTREACHED();
    return;
  }
  base::string16 name = entry->GetName();

  for (auto& observer : observer_list_)
    observer.OnProfileWillBeRemoved(profile_path);

  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  std::string key = CacheKeyFromProfilePath(profile_path);
  cache->Remove(key, NULL);
  sorted_keys_.erase(std::find(sorted_keys_.begin(), sorted_keys_.end(), key));
  profile_attributes_entries_.erase(profile_path.value());

  for (auto& observer : observer_list_)
    observer.OnProfileWasRemoved(profile_path, name);
}

size_t ProfileInfoCache::GetNumberOfProfiles() const {
  return sorted_keys_.size();
}

size_t ProfileInfoCache::GetIndexOfProfileWithPath(
    const base::FilePath& profile_path) const {
  if (profile_path.DirName() != user_data_dir_)
    return std::string::npos;
  std::string search_key = CacheKeyFromProfilePath(profile_path);
  for (size_t i = 0; i < sorted_keys_.size(); ++i) {
    if (sorted_keys_[i] == search_key)
      return i;
  }
  return std::string::npos;
}

base::string16 ProfileInfoCache::GetNameOfProfileAtIndex(size_t index) const {
  base::string16 name;
  // Unless the user has customized the profile name, we should use the
  // profile's Gaia given name, if it's available.
  if (ProfileIsUsingDefaultNameAtIndex(index)) {
    base::string16 given_name = GetGAIAGivenNameOfProfileAtIndex(index);
    name = given_name.empty() ? GetGAIANameOfProfileAtIndex(index) : given_name;
  }
  if (name.empty())
    GetInfoForProfileAtIndex(index)->GetString(kNameKey, &name);
  return name;
}

base::FilePath ProfileInfoCache::GetPathOfProfileAtIndex(size_t index) const {
  return user_data_dir_.AppendASCII(sorted_keys_[index]);
}

base::string16 ProfileInfoCache::GetUserNameOfProfileAtIndex(
    size_t index) const {
  base::string16 user_name;
  GetInfoForProfileAtIndex(index)->GetString(
      ProfileAttributesEntry::kUserNameKey, &user_name);
  return user_name;
}

const gfx::Image& ProfileInfoCache::GetAvatarIconOfProfileAtIndex(
    size_t index) const {
  if (IsUsingGAIAPictureOfProfileAtIndex(index)) {
    const gfx::Image* image = GetGAIAPictureOfProfileAtIndex(index);
    if (image)
      return *image;
  }

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  // Use the high resolution version of the avatar if it exists. Mobile and
  // ChromeOS don't need the high resolution version so no need to fetch it.
  const gfx::Image* image = GetHighResAvatarOfProfileAtIndex(index);
  if (image)
    return *image;
#endif

  int resource_id = profiles::GetDefaultAvatarIconResourceIDAtIndex(
      GetAvatarIconIndexOfProfileAtIndex(index));
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

bool ProfileInfoCache::GetBackgroundStatusOfProfileAtIndex(
    size_t index) const {
  bool background_app_status;
  if (!GetInfoForProfileAtIndex(index)->GetBoolean(
          ProfileAttributesEntry::kBackgroundAppsKey, &background_app_status)) {
    return false;
  }
  return background_app_status;
}

base::string16 ProfileInfoCache::GetGAIANameOfProfileAtIndex(
    size_t index) const {
  base::string16 name;
  GetInfoForProfileAtIndex(index)->GetString(kGAIANameKey, &name);
  return name;
}

base::string16 ProfileInfoCache::GetGAIAGivenNameOfProfileAtIndex(
    size_t index) const {
  base::string16 name;
  GetInfoForProfileAtIndex(index)->GetString(kGAIAGivenNameKey, &name);
  return name;
}

std::string ProfileInfoCache::GetGAIAIdOfProfileAtIndex(
    size_t index) const {
  std::string gaia_id;
  GetInfoForProfileAtIndex(index)->GetString(kGAIAIdKey, &gaia_id);
  return gaia_id;
}

const gfx::Image* ProfileInfoCache::GetGAIAPictureOfProfileAtIndex(
    size_t index) const {
  base::FilePath path = GetPathOfProfileAtIndex(index);
  std::string key = CacheKeyFromProfilePath(path);

  std::string file_name;
  GetInfoForProfileAtIndex(index)->GetString(
      kGAIAPictureFileNameKey, &file_name);

  // If the picture is not on disk then return NULL.
  if (file_name.empty())
    return NULL;

  base::FilePath image_path = path.AppendASCII(file_name);
  return LoadAvatarPictureFromPath(path, key, image_path);
}

bool ProfileInfoCache::IsUsingGAIAPictureOfProfileAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kUseGAIAPictureKey, &value);
  if (!value) {
    // Prefer the GAIA avatar over a non-customized avatar.
    value = ProfileIsUsingDefaultAvatarAtIndex(index) &&
        GetGAIAPictureOfProfileAtIndex(index);
  }
  return value;
}

bool ProfileInfoCache::ProfileIsSupervisedAtIndex(size_t index) const {
  return !GetSupervisedUserIdOfProfileAtIndex(index).empty();
}

bool ProfileInfoCache::ProfileIsChildAtIndex(size_t index) const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return GetSupervisedUserIdOfProfileAtIndex(index) ==
      supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool ProfileInfoCache::ProfileIsLegacySupervisedAtIndex(size_t index) const {
  return ProfileIsSupervisedAtIndex(index) && !ProfileIsChildAtIndex(index);
}

bool ProfileInfoCache::IsOmittedProfileAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsOmittedFromProfileListKey,
                                              &value);
  return value;
}

bool ProfileInfoCache::ProfileIsSigninRequiredAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kSigninRequiredKey, &value);
  return value;
}

std::string ProfileInfoCache::GetSupervisedUserIdOfProfileAtIndex(
    size_t index) const {
  std::string supervised_user_id;
  GetInfoForProfileAtIndex(index)->GetString(kSupervisedUserId,
                                             &supervised_user_id);
  return supervised_user_id;
}

bool ProfileInfoCache::ProfileIsUsingDefaultNameAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsUsingDefaultNameKey, &value);
  return value;
}

bool ProfileInfoCache::ProfileIsUsingDefaultAvatarAtIndex(size_t index) const {
  bool value = false;
  GetInfoForProfileAtIndex(index)->GetBoolean(kIsUsingDefaultAvatarKey, &value);
  return value;
}

bool ProfileInfoCache::IsGAIAPictureOfProfileAtIndexLoaded(size_t index) const {
  return cached_avatar_images_.count(
      CacheKeyFromProfilePath(GetPathOfProfileAtIndex(index)));
}

size_t ProfileInfoCache::GetAvatarIconIndexOfProfileAtIndex(size_t index)
    const {
  std::string icon_url;
  GetInfoForProfileAtIndex(index)->GetString(
      ProfileAttributesEntry::kAvatarIconKey, &icon_url);
  size_t icon_index = 0;
  if (!profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index))
    DLOG(WARNING) << "Unknown avatar icon: " << icon_url;

  return icon_index;
}

void ProfileInfoCache::SetNameOfProfileAtIndex(size_t index,
                                               const base::string16& name) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  base::string16 current_name;
  info->GetString(kNameKey, &current_name);
  if (name == current_name)
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);
  info->SetString(kNameKey, name);

  SetInfoForProfileAtIndex(index, std::move(info));

  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetAuthInfoOfProfileAtIndex(
    size_t index,
    const std::string& gaia_id,
    const base::string16& user_name) {
  // If both gaia_id and username are unchanged, abort early.
  if (gaia_id == GetGAIAIdOfProfileAtIndex(index) &&
      user_name == GetUserNameOfProfileAtIndex(index)) {
    return;
  }

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());

  info->SetString(kGAIAIdKey, gaia_id);
  info->SetString(ProfileAttributesEntry::kUserNameKey, user_name);

  SetInfoForProfileAtIndex(index, std::move(info));

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileAuthInfoChanged(profile_path);
}

void ProfileInfoCache::SetAvatarIconOfProfileAtIndex(size_t index,
                                                     size_t icon_index) {
  if (!profiles::IsDefaultAvatarIconIndex(icon_index)) {
    DLOG(WARNING) << "Unknown avatar icon index: " << icon_index;
    // switch to generic avatar
    icon_index = 0;
  }
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(ProfileAttributesEntry::kAvatarIconKey,
                  profiles::GetDefaultAvatarIconUrl(icon_index));
  SetInfoForProfileAtIndex(index, std::move(info));

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);

  if (!disable_avatar_download_for_testing_)
    DownloadHighResAvatarIfNeeded(icon_index, profile_path);

  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(profile_path);
}

void ProfileInfoCache::SetIsOmittedProfileAtIndex(size_t index,
                                                  bool is_omitted) {
  if (IsOmittedProfileAtIndex(index) == is_omitted)
    return;
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsOmittedFromProfileListKey, is_omitted);
  SetInfoForProfileAtIndex(index, std::move(info));

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileIsOmittedChanged(profile_path);
}

void ProfileInfoCache::SetSupervisedUserIdOfProfileAtIndex(
    size_t index,
    const std::string& id) {
  if (GetSupervisedUserIdOfProfileAtIndex(index) == id)
    return;
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kSupervisedUserId, id);
  SetInfoForProfileAtIndex(index, std::move(info));

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileSupervisedUserIdChanged(profile_path);
}

void ProfileInfoCache::SetBackgroundStatusOfProfileAtIndex(
    size_t index,
    bool running_background_apps) {
  if (GetBackgroundStatusOfProfileAtIndex(index) == running_background_apps)
    return;
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(ProfileAttributesEntry::kBackgroundAppsKey,
                   running_background_apps);
  SetInfoForProfileAtIndex(index, std::move(info));
}

void ProfileInfoCache::SetGAIANameOfProfileAtIndex(size_t index,
                                                   const base::string16& name) {
  if (name == GetGAIANameOfProfileAtIndex(index))
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIANameKey, name);
  SetInfoForProfileAtIndex(index, std::move(info));
  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetGAIAGivenNameOfProfileAtIndex(
    size_t index,
    const base::string16& name) {
  if (name == GetGAIAGivenNameOfProfileAtIndex(index))
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIAGivenNameKey, name);
  SetInfoForProfileAtIndex(index, std::move(info));
  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  UpdateSortForProfileIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetGAIAPictureOfProfileAtIndex(size_t index,
                                                      const gfx::Image* image) {
  base::FilePath path = GetPathOfProfileAtIndex(index);
  std::string key = CacheKeyFromProfilePath(path);

  // Delete the old bitmap from cache.
  cached_avatar_images_.erase(key);

  std::string old_file_name;
  GetInfoForProfileAtIndex(index)->GetString(
      kGAIAPictureFileNameKey, &old_file_name);
  std::string new_file_name;

  if (!image) {
    // Delete the old bitmap from disk.
    if (!old_file_name.empty()) {
      base::FilePath image_path = path.AppendASCII(old_file_name);
      file_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(&DeleteBitmap, image_path));
    }
  } else {
    // Save the new bitmap to disk.
    new_file_name =
        old_file_name.empty() ? profiles::kGAIAPictureFileName : old_file_name;
    base::FilePath image_path = path.AppendASCII(new_file_name);
    SaveAvatarImageAtPath(
        GetPathOfProfileAtIndex(index), image, key, image_path);
  }

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kGAIAPictureFileNameKey, new_file_name);
  SetInfoForProfileAtIndex(index, std::move(info));

  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(path);
}

void ProfileInfoCache::SetIsUsingGAIAPictureOfProfileAtIndex(size_t index,
                                                             bool value) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kUseGAIAPictureKey, value);
  SetInfoForProfileAtIndex(index, std::move(info));

  base::FilePath profile_path = GetPathOfProfileAtIndex(index);
  for (auto& observer : observer_list_)
    observer.OnProfileAvatarChanged(profile_path);
}

void ProfileInfoCache::SetProfileSigninRequiredAtIndex(size_t index,
                                                       bool value) {
  if (value == ProfileIsSigninRequiredAtIndex(index))
    return;

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kSigninRequiredKey, value);
  SetInfoForProfileAtIndex(index, std::move(info));
  NotifyIsSigninRequiredChanged(GetPathOfProfileAtIndex(index));
}

void ProfileInfoCache::SetProfileIsUsingDefaultNameAtIndex(
    size_t index, bool value) {
  if (value == ProfileIsUsingDefaultNameAtIndex(index))
    return;

  base::string16 old_display_name = GetNameOfProfileAtIndex(index);

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsUsingDefaultNameKey, value);
  SetInfoForProfileAtIndex(index, std::move(info));

  base::string16 new_display_name = GetNameOfProfileAtIndex(index);
  const base::FilePath profile_path = GetPathOfProfileAtIndex(index);

  if (old_display_name != new_display_name) {
    for (auto& observer : observer_list_)
      observer.OnProfileNameChanged(profile_path, old_display_name);
  }
}

void ProfileInfoCache::SetProfileIsUsingDefaultAvatarAtIndex(
    size_t index, bool value) {
  if (value == ProfileIsUsingDefaultAvatarAtIndex(index))
    return;

  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetBoolean(kIsUsingDefaultAvatarKey, value);
  SetInfoForProfileAtIndex(index, std::move(info));
}

void ProfileInfoCache::NotifyIsSigninRequiredChanged(
    const base::FilePath& profile_path) {
  for (auto& observer : observer_list_)
    observer.OnProfileSigninRequiredChanged(profile_path);
}

const base::FilePath& ProfileInfoCache::GetUserDataDir() const {
  return user_data_dir_;
}

// static
void ProfileInfoCache::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kProfileInfoCache);
}

const base::DictionaryValue* ProfileInfoCache::GetInfoForProfileAtIndex(
    size_t index) const {
  DCHECK_LT(index, GetNumberOfProfiles());
  const base::DictionaryValue* cache =
      prefs_->GetDictionary(prefs::kProfileInfoCache);
  const base::DictionaryValue* info = NULL;
  cache->GetDictionaryWithoutPathExpansion(sorted_keys_[index], &info);
  return info;
}

void ProfileInfoCache::SetInfoForProfileAtIndex(
    size_t index,
    std::unique_ptr<base::DictionaryValue> info) {
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  cache->SetWithoutPathExpansion(sorted_keys_[index], std::move(info));
}

std::string ProfileInfoCache::CacheKeyFromProfilePath(
    const base::FilePath& profile_path) const {
  DCHECK(user_data_dir_ == profile_path.DirName());
  base::FilePath base_name = profile_path.BaseName();
  return base_name.MaybeAsASCII();
}

std::vector<std::string>::iterator ProfileInfoCache::FindPositionForProfile(
    const std::string& search_key,
    const base::string16& search_name) {
  base::string16 search_name_l = base::i18n::ToLower(search_name);
  for (size_t i = 0; i < GetNumberOfProfiles(); ++i) {
    base::string16 name_l = base::i18n::ToLower(GetNameOfProfileAtIndex(i));
    int name_compare = search_name_l.compare(name_l);
    if (name_compare < 0)
      return sorted_keys_.begin() + i;
    if (name_compare == 0) {
      int key_compare = search_key.compare(sorted_keys_[i]);
      if (key_compare < 0)
        return sorted_keys_.begin() + i;
    }
  }
  return sorted_keys_.end();
}

void ProfileInfoCache::UpdateSortForProfileIndex(size_t index) {
  base::string16 name = GetNameOfProfileAtIndex(index);

  // Remove and reinsert key in |sorted_keys_| to alphasort.
  std::string key = CacheKeyFromProfilePath(GetPathOfProfileAtIndex(index));
  auto key_it = std::find(sorted_keys_.begin(), sorted_keys_.end(), key);
  DCHECK(key_it != sorted_keys_.end());
  sorted_keys_.erase(key_it);
  sorted_keys_.insert(FindPositionForProfile(key, name), key);
}

const gfx::Image* ProfileInfoCache::GetHighResAvatarOfProfileAtIndex(
    size_t index) const {
  const size_t avatar_index = GetAvatarIconIndexOfProfileAtIndex(index);

  // If this is the placeholder avatar, it is already included in the
  // resources, so it doesn't need to be downloaded.
  if (avatar_index == profiles::GetPlaceholderAvatarIndex()) {
    return &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  const std::string file_name =
      profiles::GetDefaultAvatarIconFileNameAtIndex(avatar_index);
  const base::FilePath image_path =
      profiles::GetPathOfHighResAvatarAtIndex(avatar_index);
  return LoadAvatarPictureFromPath(GetPathOfProfileAtIndex(index), file_name,
                                   image_path);
}

void ProfileInfoCache::MigrateLegacyProfileNamesAndDownloadAvatars() {
  // Only do this on desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Migrate any legacy default profile names ("First user", "Default Profile")
  // to new style default names ("Person 1").
  const base::string16 default_profile_name = base::i18n::ToLower(
      l10n_util::GetStringUTF16(IDS_DEFAULT_PROFILE_NAME));
  const base::string16 default_legacy_profile_name = base::i18n::ToLower(
      l10n_util::GetStringUTF16(IDS_LEGACY_DEFAULT_PROFILE_NAME));

  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    DownloadHighResAvatarIfNeeded(entry->GetAvatarIconIndex(),
                                  entry->GetPath());

    // Rename the necessary profiles.
    base::string16 name = base::i18n::ToLower(entry->GetName());
    if (name == default_profile_name || name == default_legacy_profile_name) {
      entry->SetIsUsingDefaultName(true);
      entry->SetName(ChooseNameForNewProfile(entry->GetAvatarIconIndex()));
    }
  }
#endif
}

void ProfileInfoCache::RemoveDeprecatedStatistics() {
  for (size_t i = 0; i < GetNumberOfProfiles(); i++) {
    if (GetInfoForProfileAtIndex(i)->HasKey(kStatsBookmarksKeyDeprecated)) {
      auto info = GetInfoForProfileAtIndex(i)->CreateDeepCopy();
      info->Remove(kStatsBookmarksKeyDeprecated, nullptr);
      info->Remove(kStatsBrowsingHistoryKeyDeprecated, nullptr);
      info->Remove(kStatsPasswordsKeyDeprecated, nullptr);
      info->Remove(kStatsSettingsKeyDeprecated, nullptr);
      SetInfoForProfileAtIndex(i, std::move(info));
    }
  }
}

void ProfileInfoCache::AddProfile(const base::FilePath& profile_path,
                                  const base::string16& name,
                                  const std::string& gaia_id,
                                  const base::string16& user_name,
                                  size_t icon_index,
                                  const std::string& supervised_user_id,
                                  const AccountId& account_id) {
  AddProfileToCache(profile_path, name, gaia_id, user_name, icon_index,
                    supervised_user_id, account_id);
}

void ProfileInfoCache::RemoveProfileByAccountId(const AccountId& account_id) {
  for (size_t i = 0; i < GetNumberOfProfiles(); i++) {
    std::string account_id_key;
    std::string gaia_id;
    std::string user_name;
    const base::DictionaryValue* info = GetInfoForProfileAtIndex(i);
    if ((account_id.HasAccountIdKey() &&
         info->GetString(kAccountIdKey, &account_id_key) &&
         account_id_key == account_id.GetAccountIdKey()) ||
        (info->GetString(kGAIAIdKey, &gaia_id) && !gaia_id.empty() &&
         account_id.GetGaiaId() == gaia_id) ||
        (info->GetString(ProfileAttributesEntry::kUserNameKey, &user_name) &&
         !user_name.empty() && account_id.GetUserEmail() == user_name)) {
      RemoveProfile(GetPathOfProfileAtIndex(i));
      return;
    }
  }
  LOG(ERROR) << "Failed to remove profile.info_cache entry for account type "
             << static_cast<int>(account_id.GetAccountType())
             << ": matching entry not found.";
}

void ProfileInfoCache::RemoveProfile(const base::FilePath& profile_path) {
  DeleteProfileFromCache(profile_path);
}

bool ProfileInfoCache::GetProfileAttributesWithPath(
    const base::FilePath& path, ProfileAttributesEntry** entry) {
  const auto entry_iter = profile_attributes_entries_.find(path.value());
  if (entry_iter == profile_attributes_entries_.end())
    return false;

  std::unique_ptr<ProfileAttributesEntry>& current_entry = entry_iter->second;
  if (!current_entry) {
    // The profile info is in the cache but its entry isn't created yet, insert
    // it in the map.
    current_entry.reset(new ProfileAttributesEntry());
    current_entry->Initialize(this, path, prefs_);
  }

  *entry = current_entry.get();
  return true;
}

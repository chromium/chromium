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
#include "build/chromeos_buildflags.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

namespace {

const char kIsUsingDefaultAvatarKey[] = "is_using_default_avatar";
const char kUseGAIAPictureKey[] = "use_gaia_picture";
const char kGAIAPictureFileNameKey[] = "gaia_picture_file_name";
const char kLastDownloadedGAIAPictureUrlWithSizeKey[] =
    "last_downloaded_gaia_picture_url_with_size";
const char kAccountIdKey[] = "account_id_key";
const char kProfileCountLastUpdatePref[] = "profile.profile_counts_reported";
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
const char kLegacyProfileNameMigrated[] = "legacy.profile.name.migrated";
bool migration_enabled_for_testing = false;
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

void DeleteBitmap(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::DeleteFile(image_path);
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
    base::DictionaryValue* info = nullptr;
    cache->GetDictionaryWithoutPathExpansion(it.key(), &info);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && !defined(OS_ANDROID) && \
    !BUILDFLAG(IS_CHROMEOS_ASH)
    std::string supervised_user_id;
    info->GetString(ProfileAttributesEntry::kSupervisedUserId,
                    &supervised_user_id);
    // Silently ignore legacy supervised user profiles.
    if (!supervised_user_id.empty() &&
        supervised_user_id != supervised_users::kChildAccountSUID) {
      continue;
    }
#endif
    base::string16 name;
    info->GetString(ProfileAttributesEntry::kNameKey, &name);
    keys_.push_back(it.key());
    profile_attributes_entries_[user_data_dir_.AppendASCII(it.key()).value()] =
        std::unique_ptr<ProfileAttributesEntry>(nullptr);

    bool using_default_name;
    if (!info->GetBoolean(ProfileAttributesEntry::kIsUsingDefaultNameKey,
                          &using_default_name)) {
      // If the preference hasn't been set, and the name is default, assume
      // that the user hasn't done this on purpose.
      // |include_check_for_legacy_profile_name| is true as this is an old
      // pre-existing profile and might have a legacy default profile name.
      using_default_name = IsDefaultProfileName(
          name, /*include_check_for_legacy_profile_name=*/true);
      info->SetBoolean(ProfileAttributesEntry::kIsUsingDefaultNameKey,
                       using_default_name);
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
    DownloadAvatars();

#if !defined(OS_ANDROID)
  LoadGAIAPictureIfNeeded();
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  bool migrate_legacy_profile_names =
      (!prefs_->GetBoolean(kLegacyProfileNameMigrated) ||
       migration_enabled_for_testing);
  if (migrate_legacy_profile_names) {
    MigrateLegacyProfileNamesAndRecomputeIfNeeded();
    prefs_->SetBoolean(kLegacyProfileNameMigrated, true);
  }

  repeating_timer_ = std::make_unique<signin::PersistentRepeatingTimer>(
      prefs_, kProfileCountLastUpdatePref, base::TimeDelta::FromHours(24),
      base::BindRepeating(&ProfileMetrics::LogNumberOfProfiles, this));
  repeating_timer_->Start();
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

ProfileInfoCache::~ProfileInfoCache() = default;

void ProfileInfoCache::AddProfileToCache(const base::FilePath& profile_path,
                                         const base::string16& name,
                                         const std::string& gaia_id,
                                         const base::string16& user_name,
                                         bool is_consented_primary_account,
                                         size_t icon_index,
                                         const std::string& supervised_user_id,
                                         const AccountId& account_id) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && !defined(OS_ANDROID) && \
    !BUILDFLAG(IS_CHROMEOS_ASH)
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
  info->SetString(ProfileAttributesEntry::kNameKey, name);
  info->SetString(ProfileAttributesEntry::kGAIAIdKey, gaia_id);
  info->SetString(ProfileAttributesEntry::kUserNameKey, user_name);
  DCHECK(!is_consented_primary_account || !gaia_id.empty() ||
         !user_name.empty());
  info->SetBoolean(ProfileAttributesEntry::kIsConsentedPrimaryAccountKey,
                   is_consented_primary_account);
  info->SetString(ProfileAttributesEntry::kAvatarIconKey,
                  profiles::GetDefaultAvatarIconUrl(icon_index));
  // Default value for whether background apps are running is false.
  info->SetBoolean(ProfileAttributesEntry::kBackgroundAppsKey, false);
  info->SetString(ProfileAttributesEntry::kSupervisedUserId,
                  supervised_user_id);
  info->SetBoolean(ProfileAttributesEntry::kIsOmittedFromProfileListKey,
                   !supervised_user_id.empty());
  info->SetBoolean(ProfileAttributesEntry::kProfileIsEphemeral, false);
  info->SetBoolean(ProfileAttributesEntry::kProfileIsGuest, false);
  // Either the user has provided a name manually on purpose, and in this case
  // we should not check for legacy profile names or this a new profile but then
  // it is not a legacy name, so we dont need to check for legacy names.
  info->SetBoolean(ProfileAttributesEntry::kIsUsingDefaultNameKey,
                   IsDefaultProfileName(
                       name, /*include_check_for_legacy_profile_name*/ false));
  // Assume newly created profiles use a default avatar.
  info->SetBoolean(kIsUsingDefaultAvatarKey, true);
  if (account_id.HasAccountIdKey())
    info->SetString(kAccountIdKey, account_id.GetAccountIdKey());
  cache->SetWithoutPathExpansion(key, std::move(info));
  keys_.push_back(key);
  profile_attributes_entries_[user_data_dir_.AppendASCII(key).value()] =
      std::unique_ptr<ProfileAttributesEntry>();

  if (!disable_avatar_download_for_testing_)
    DownloadHighResAvatarIfNeeded(icon_index, profile_path);

  NotifyIfProfileNamesHaveChanged();
  for (auto& observer : observer_list_)
    observer.OnProfileAdded(profile_path);
}

void ProfileInfoCache::DisableProfileMetricsForTesting() {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  repeating_timer_.reset();
#endif
}

void ProfileInfoCache::NotifyIfProfileNamesHaveChanged() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::string16 old_display_name = entry->GetLastNameToDisplay();
    if (entry->HasProfileNameChanged()) {
      for (auto& observer : observer_list_)
        observer.OnProfileNameChanged(entry->GetPath(), old_display_name);
    }
  }
}

void ProfileInfoCache::NotifyProfileSupervisedUserIdChanged(
    const base::FilePath& profile_path) {
  for (auto& observer : observer_list_)
    observer.OnProfileSupervisedUserIdChanged(profile_path);
}

void ProfileInfoCache::NotifyProfileIsOmittedChanged(
    const base::FilePath& profile_path) {
  for (auto& observer : observer_list_)
    observer.OnProfileIsOmittedChanged(profile_path);
}

void ProfileInfoCache::NotifyProfileThemeColorsChanged(
    const base::FilePath& profile_path) {
  for (auto& observer : observer_list_)
    observer.OnProfileThemeColorsChanged(profile_path);
}

void ProfileInfoCache::NotifyProfileHostedDomainChanged(
    const base::FilePath& profile_path) {
  for (auto& observer : observer_list_)
    observer.OnProfileHostedDomainChanged(profile_path);
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
  cache->Remove(key, nullptr);
  keys_.erase(std::find(keys_.begin(), keys_.end(), key));
  profile_attributes_entries_.erase(profile_path.value());

  NotifyIfProfileNamesHaveChanged();
  for (auto& observer : observer_list_) {
    observer.OnProfileWasRemoved(profile_path, name);
  }
}

size_t ProfileInfoCache::GetNumberOfProfiles(bool include_guest_profile) const {
// Ephemeral Guest profile is registered in profile attributes storage,
// because if Chrome crashes we need the registry to find and delete it.
// But it should not be counted as a regular profile.
#if !defined(OS_ANDROID)
  if (!include_guest_profile) {
    for (auto& profile : profile_attributes_entries_) {
      if (profile.second && profile.second->IsGuest())
        return keys_.size() - 1;
    }
  }
#endif

  return keys_.size();
}

size_t ProfileInfoCache::GetIndexOfProfileWithPath(
    const base::FilePath& profile_path) const {
  if (profile_path.DirName() != user_data_dir_)
    return std::string::npos;
  std::string search_key = CacheKeyFromProfilePath(profile_path);
  for (size_t i = 0; i < keys_.size(); ++i) {
    if (keys_[i] == search_key)
      return i;
  }
  return std::string::npos;
}

base::FilePath ProfileInfoCache::GetPathOfProfileAtIndex(size_t index) const {
  return user_data_dir_.AppendASCII(keys_[index]);
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
    return nullptr;

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

void ProfileInfoCache::NotifyProfileAuthInfoChanged(
    const base::FilePath& profile_path) {
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

std::string
ProfileInfoCache::GetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(
    size_t index) const {
  std::string current_gaia_image_url;
  GetInfoForProfileAtIndex(index)->GetString(
      kLastDownloadedGAIAPictureUrlWithSizeKey, &current_gaia_image_url);
  return current_gaia_image_url;
}

void ProfileInfoCache::SetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(
    size_t index,
    const std::string& image_url_with_size) {
  std::unique_ptr<base::DictionaryValue> info(
      GetInfoForProfileAtIndex(index)->DeepCopy());
  info->SetString(kLastDownloadedGAIAPictureUrlWithSizeKey,
                  image_url_with_size);
  SetInfoForProfileAtIndex(index, std::move(info));
}

bool ProfileInfoCache::ShouldUpdateGAIAPictureOfProfileAtIndex(
    size_t index,
    const std::string& old_file_name,
    const std::string& key,
    const std::string& image_url_with_size,
    bool image_is_empty) const {
  if (old_file_name.empty() && image_is_empty) {
    // On Windows, Taskbar and Desktop icons are refreshed every time
    // |OnProfileAvatarChanged| notification is fired.
    // Updating from an empty image to a null image is a no-op and it is
    // important to avoid firing |OnProfileAvatarChanged| in this case.
    // See http://crbug.com/900374
    DCHECK_EQ(0U, cached_avatar_images_.count(key));
    return false;
  }

  std::string current_gaia_image_url =
      GetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(index);
  if (old_file_name.empty() || image_is_empty ||
      current_gaia_image_url != image_url_with_size) {
    return true;
  }
  const gfx::Image* gaia_picture = GetGAIAPictureOfProfileAtIndex(index);
  if (gaia_picture && !gaia_picture->IsEmpty()) {
    return false;
  }

  // We either did not load the GAIA image or we failed to. In that case, only
  // update if the GAIA picture is used as the profile avatar.
  return ProfileIsUsingDefaultAvatarAtIndex(index) ||
         IsUsingGAIAPictureOfProfileAtIndex(index);
}

void ProfileInfoCache::SetGAIAPictureOfProfileAtIndex(
    size_t index,
    const std::string& image_url_with_size,
    gfx::Image image) {
  base::FilePath path = GetPathOfProfileAtIndex(index);
  std::string key = CacheKeyFromProfilePath(path);

  std::string old_file_name;
  GetInfoForProfileAtIndex(index)->GetString(kGAIAPictureFileNameKey,
                                             &old_file_name);

  if (!ShouldUpdateGAIAPictureOfProfileAtIndex(
          index, old_file_name, key, image_url_with_size, image.IsEmpty())) {
    return;
  }

  // Delete the old bitmap from cache.
  cached_avatar_images_.erase(key);
  std::string new_file_name;
  if (image.IsEmpty()) {
    // Delete the old bitmap from disk.
    base::FilePath image_path = path.AppendASCII(old_file_name);
    file_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&DeleteBitmap, image_path));
    SetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex(index,
                                                            std::string());
  } else {
    // Save the new bitmap to disk.
    new_file_name =
        old_file_name.empty()
            ? base::FilePath(profiles::kGAIAPictureFileName).MaybeAsASCII()
            : old_file_name;
    base::FilePath image_path = path.AppendASCII(new_file_name);
    SaveAvatarImageAtPath(
        GetPathOfProfileAtIndex(index), image, key, image_path,
        base::BindOnce(
            &ProfileInfoCache::
                SetLastDownloadedGAIAPictureUrlWithSizeOfProfileAtIndex,
            weak_factory_.GetWeakPtr(), index, image_url_with_size));
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
  registry->RegisterTimePref(kProfileCountLastUpdatePref, base::Time());
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(kLegacyProfileNameMigrated, false);
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

const base::DictionaryValue* ProfileInfoCache::GetInfoForProfileAtIndex(
    size_t index) const {
  DCHECK_LT(index, GetNumberOfProfiles(true));
  const base::DictionaryValue* cache =
      prefs_->GetDictionary(prefs::kProfileInfoCache);
  const base::DictionaryValue* info = nullptr;
  cache->GetDictionaryWithoutPathExpansion(keys_[index], &info);
  return info;
}

void ProfileInfoCache::SetInfoForProfileAtIndex(
    size_t index,
    std::unique_ptr<base::DictionaryValue> info) {
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  cache->SetWithoutPathExpansion(keys_[index], std::move(info));
}

std::string ProfileInfoCache::CacheKeyFromProfilePath(
    const base::FilePath& profile_path) const {
  DCHECK(user_data_dir_ == profile_path.DirName());
  base::FilePath base_name = profile_path.BaseName();
  return base_name.MaybeAsASCII();
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

#if !defined(OS_ANDROID)
void ProfileInfoCache::LoadGAIAPictureIfNeeded() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->GetSigninState() == SigninState::kNotSignedIn)
      continue;

    bool is_using_GAIA_picture = entry->GetBool(kUseGAIAPictureKey);
    bool is_using_default_avatar = entry->IsUsingDefaultAvatar();
    // Load from disk into memory GAIA picture if it exists.
    if (is_using_GAIA_picture || is_using_default_avatar)
      entry->GetGAIAPicture();
  }
}
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileInfoCache::MigrateLegacyProfileNamesAndRecomputeIfNeeded() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (size_t i = 0; i < entries.size(); i++) {
    base::string16 profile_name = entries[i]->GetLocalProfileName();
    if (!entries[i]->IsUsingDefaultName())
      continue;

    // Migrate any legacy profile names ("First user", "Default Profile",
    // "Saratoga", ...) to new style default names Person %n ("Person 1").
    if (!IsDefaultProfileName(
            profile_name, /*include_check_for_legacy_profile_name=*/false)) {
      entries[i]->SetLocalProfileName(
          ChooseNameForNewProfile(entries[i]->GetAvatarIconIndex()));
      continue;
    }

    if (i == (entries.size() - 1))
      continue;

    // Current profile name is Person %n.
    // Rename duplicate default profile names, e.g.: Person 1, Person 1 to
    // Person 1, Person 2.
    for (size_t j = i + 1; j < entries.size(); j++) {
      if (profile_name == entries[j]->GetLocalProfileName()) {
        entries[j]->SetLocalProfileName(
            ChooseNameForNewProfile(entries[j]->GetAvatarIconIndex()));
      }
    }
  }
}

// static
void ProfileInfoCache::SetLegacyProfileMigrationForTesting(bool value) {
  migration_enabled_for_testing = value;
}
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

void ProfileInfoCache::DownloadAvatars() {
#if !defined(OS_ANDROID)
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    DownloadHighResAvatarIfNeeded(entry->GetAvatarIconIndex(),
                                  entry->GetPath());
  }
#endif
}

void ProfileInfoCache::AddProfile(const base::FilePath& profile_path,
                                  const base::string16& name,
                                  const std::string& gaia_id,
                                  const base::string16& user_name,
                                  bool is_consented_primary_account,
                                  size_t icon_index,
                                  const std::string& supervised_user_id,
                                  const AccountId& account_id) {
  AddProfileToCache(profile_path, name, gaia_id, user_name,
                    is_consented_primary_account, icon_index,
                    supervised_user_id, account_id);
}

void ProfileInfoCache::RemoveProfileByAccountId(const AccountId& account_id) {
  for (size_t i = 0; i < GetNumberOfProfiles(true); i++) {
    std::string account_id_key;
    std::string gaia_id;
    std::string user_name;
    const base::DictionaryValue* info = GetInfoForProfileAtIndex(i);
    if ((account_id.HasAccountIdKey() &&
         info->GetString(kAccountIdKey, &account_id_key) &&
         account_id_key == account_id.GetAccountIdKey()) ||
        (info->GetString(ProfileAttributesEntry::kGAIAIdKey, &gaia_id) &&
         !gaia_id.empty() && account_id.GetGaiaId() == gaia_id) ||
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

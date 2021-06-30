// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace {

const char kProfileCountLastUpdatePref[] = "profile.profile_counts_reported";
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
const char kLegacyProfileNameMigrated[] = "legacy.profile.name.migrated";
bool migration_enabled_for_testing = false;
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

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
    std::u16string name;
    info->GetString(ProfileAttributesEntry::kNameKey, &name);

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
    if (!info->HasKey(ProfileAttributesEntry::kIsUsingDefaultAvatarKey)) {
      info->SetBoolean(ProfileAttributesEntry::kIsUsingDefaultAvatarKey,
                       using_default_name);
    }

    // `info` may become invalid after this call.
    // Profiles loaded from disk can never be omitted.
    InitEntryWithKey(it.key(), /*is_omitted=*/false);
  }

  // A profile name can depend on other profile names. Do an additional pass to
  // update last used profile names once all profiles are initialized.
  for (ProfileAttributesEntry* entry :
       GetAllProfilesAttributes(/*include_guest_profile=*/true)) {
    entry->InitializeLastNameToDisplay();
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

void ProfileInfoCache::AddProfileToCache(ProfileAttributesInitParams params) {
  std::string key = CacheKeyFromProfilePath(params.profile_path);
  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();

  std::unique_ptr<base::DictionaryValue> info(new base::DictionaryValue);
  info->SetString(ProfileAttributesEntry::kNameKey, params.profile_name);
  info->SetString(ProfileAttributesEntry::kGAIAIdKey, params.gaia_id);
  info->SetString(ProfileAttributesEntry::kUserNameKey, params.user_name);
  DCHECK(!params.is_consented_primary_account || !params.gaia_id.empty() ||
         !params.user_name.empty());
  info->SetBoolean(ProfileAttributesEntry::kIsConsentedPrimaryAccountKey,
                   params.is_consented_primary_account);
  info->SetString(ProfileAttributesEntry::kAvatarIconKey,
                  profiles::GetDefaultAvatarIconUrl(params.icon_index));
  // Default value for whether background apps are running is false.
  info->SetBoolean(ProfileAttributesEntry::kBackgroundAppsKey, false);
  info->SetString(ProfileAttributesEntry::kSupervisedUserId,
                  params.supervised_user_id);
  info->SetBoolean(ProfileAttributesEntry::kProfileIsEphemeral,
                   params.is_ephemeral);
  info->SetBoolean(ProfileAttributesEntry::kProfileIsGuest, params.is_guest);
  // Either the user has provided a name manually on purpose, and in this case
  // we should not check for legacy profile names or this a new profile but then
  // it is not a legacy name, so we dont need to check for legacy names.
  info->SetBoolean(
      ProfileAttributesEntry::kIsUsingDefaultNameKey,
      IsDefaultProfileName(params.profile_name,
                           /*include_check_for_legacy_profile_name*/ false));
  // Assume newly created profiles use a default avatar.
  info->SetBoolean(ProfileAttributesEntry::kIsUsingDefaultAvatarKey, true);
  if (params.account_id.HasAccountIdKey())
    info->SetString(ProfileAttributesEntry::kAccountIdKey,
                    params.account_id.GetAccountIdKey());
  info->SetBoolKey(prefs::kSignedInWithCredentialProvider,
                   params.is_signed_in_with_credential_provider);
  cache->SetKey(key, base::Value::FromUniquePtrValue(std::move(info)));
  ProfileAttributesEntry* entry = InitEntryWithKey(key, params.is_omitted);
  entry->InitializeLastNameToDisplay();

  // `OnProfileAdded()` must be the first observer method being called right
  // after a new profile is added to cache.
  for (auto& observer : observer_list_)
    observer.OnProfileAdded(params.profile_path);

  if (!disable_avatar_download_for_testing_)
    DownloadHighResAvatarIfNeeded(params.icon_index, params.profile_path);

  NotifyIfProfileNamesHaveChanged();
}

void ProfileInfoCache::DisableProfileMetricsForTesting() {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  repeating_timer_.reset();
#endif
}

void ProfileInfoCache::DeleteProfileFromCache(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry = GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    NOTREACHED();
    return;
  }

  std::u16string name = entry->GetName();

  for (auto& observer : observer_list_)
    observer.OnProfileWillBeRemoved(profile_path);

  DictionaryPrefUpdate update(prefs_, prefs::kProfileInfoCache);
  base::DictionaryValue* cache = update.Get();
  std::string key = CacheKeyFromProfilePath(profile_path);
  cache->Remove(key, nullptr);
  profile_attributes_entries_.erase(profile_path.value());

  // `OnProfileWasRemoved()` must be the first observer method being called
  // right after a profile was removed from cache.
  for (auto& observer : observer_list_) {
    observer.OnProfileWasRemoved(profile_path, name);
  }

  NotifyIfProfileNamesHaveChanged();
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

std::string ProfileInfoCache::CacheKeyFromProfilePath(
    const base::FilePath& profile_path) const {
  DCHECK(user_data_dir_ == profile_path.DirName());
  base::FilePath base_name = profile_path.BaseName();
  return base_name.MaybeAsASCII();
}

#if !defined(OS_ANDROID)
void ProfileInfoCache::LoadGAIAPictureIfNeeded() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->GetSigninState() == SigninState::kNotSignedIn)
      continue;

    bool is_using_GAIA_picture =
        entry->GetBool(ProfileAttributesEntry::kUseGAIAPictureKey);
    bool is_using_default_avatar = entry->IsUsingDefaultAvatar();
    // Load from disk into memory GAIA picture if it exists.
    if (is_using_GAIA_picture || is_using_default_avatar)
      entry->GetGAIAPicture();
  }
}
#endif

ProfileAttributesEntry* ProfileInfoCache::InitEntryWithKey(
    const std::string& key,
    bool is_omitted) {
  base::FilePath path = user_data_dir_.AppendASCII(key);
  DCHECK(!base::Contains(profile_attributes_entries_, path.value()));
  ProfileAttributesEntry* new_entry =
      &profile_attributes_entries_[path.value()];
  new_entry->Initialize(this, path, prefs_);
  new_entry->SetIsOmittedInternal(is_omitted);
  return new_entry;
}

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileInfoCache::MigrateLegacyProfileNamesAndRecomputeIfNeeded() {
  std::vector<ProfileAttributesEntry*> entries = GetAllProfilesAttributes();
  for (size_t i = 0; i < entries.size(); i++) {
    std::u16string profile_name = entries[i]->GetLocalProfileName();
    if (!entries[i]->IsUsingDefaultName())
      continue;

    // Migrate any legacy profile names ("First user", "Default Profile",
    // "Saratoga", ...) to new style default names Person %n ("Person 1").
    if (!IsDefaultProfileName(
            profile_name, /*include_check_for_legacy_profile_name=*/false)) {
      entries[i]->SetLocalProfileName(
          ChooseNameForNewProfile(entries[i]->GetAvatarIconIndex()),
          /*is_default_name=*/true);
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
            ChooseNameForNewProfile(entries[j]->GetAvatarIconIndex()),
            /*is_default_name=*/true);
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

void ProfileInfoCache::AddProfile(ProfileAttributesInitParams params) {
  AddProfileToCache(std::move(params));
}

void ProfileInfoCache::RemoveProfileByAccountId(const AccountId& account_id) {
  for (ProfileAttributesEntry* entry : GetAllProfilesAttributes(true)) {
    bool account_id_keys_match =
        account_id.HasAccountIdKey() &&
        account_id.GetAccountIdKey() == entry->GetAccountIdKey();
    bool gaia_ids_match = !entry->GetGAIAId().empty() &&
                          account_id.GetGaiaId() == entry->GetGAIAId();
    bool user_names_match =
        !entry->GetUserName().empty() &&
        account_id.GetUserEmail() == base::UTF16ToUTF8(entry->GetUserName());
    if (account_id_keys_match || gaia_ids_match || user_names_match) {
      RemoveProfile(entry->GetPath());
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

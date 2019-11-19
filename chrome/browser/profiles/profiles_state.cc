// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browsing_data_remover.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state/login_state.h"
#else
#include <algorithm>
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "components/signin/public/base/signin_pref_names.h"
#endif

namespace profiles {

bool IsMultipleProfilesEnabled() {
#if defined(OS_ANDROID)
  return false;
#endif
  return true;
}

base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir) {
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir =
      default_profile_dir.AppendASCII(chrome::kInitialProfile);
  return default_profile_dir;
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  // Preferences about global profile information.
  registry->RegisterStringPref(prefs::kProfileLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
  registry->RegisterListPref(prefs::kProfilesLastActive);
  registry->RegisterListPref(prefs::kProfilesDeleted);

  // Preferences about the user manager.
  registry->RegisterBooleanPref(prefs::kBrowserGuestModeEnabled, true);
  registry->RegisterBooleanPref(prefs::kBrowserGuestModeEnforced, false);
  registry->RegisterBooleanPref(prefs::kBrowserAddPersonEnabled, true);
  registry->RegisterBooleanPref(prefs::kForceBrowserSignin, false);
}

void SetLastUsedProfile(const std::string& profile_dir) {
  // We should never be saving the System Profile as the last one used since it
  // shouldn't have a browser.
  if (profile_dir == base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe())
    return;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->SetString(prefs::kProfileLastUsed, profile_dir);
}

#if !defined(OS_ANDROID)
base::string16 GetAvatarNameForProfile(const base::FilePath& profile_path) {
  if (profile_path == ProfileManager::GetGuestProfilePath()) {
    return l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  }

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  ProfileAttributesEntry* entry;
  if (!storage.GetProfileAttributesWithPath(profile_path, &entry))
    return l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);

  const base::string16 profile_name_to_display = entry->GetName();
  // If the user has set their local profile name on purpose.
  bool is_default_name = entry->IsUsingDefaultName();
  if (!is_default_name)
    return profile_name_to_display;

  // The profile is signed in and has a GAIA name.
  const base::string16 gaia_name_to_display = entry->GetGAIANameToDisplay();
  if (!gaia_name_to_display.empty())
    return profile_name_to_display;

  // For a single profile that does not have a GAIA name
  // (most probably not signed in), with a default name
  // (i.e. of the form Person %d) not manually set, it should display
  // IDS_SINGLE_PROFILE_DISPLAY_NAME.
  if (storage.GetNumberOfProfiles() == 1u)
    return l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);

  // If the profile is signed in but does not have a GAIA name nor a custom
  // local profile name, show the email address if it exists.
  // Otherwise, show the profile name which is expected to be the local
  // profile name.
  const base::string16 email = entry->GetUserName();
  return email.empty() ? profile_name_to_display : email;
}

#if !defined(OS_CHROMEOS)
base::string16 GetProfileSwitcherTextForItem(const AvatarMenu::Item& item) {
  if (item.legacy_supervised) {
    return l10n_util::GetStringFUTF16(
        IDS_LEGACY_SUPERVISED_USER_NEW_AVATAR_LABEL, item.name);
  }
  if (item.child_account)
    return l10n_util::GetStringFUTF16(IDS_CHILD_AVATAR_LABEL, item.name);
  return item.name;
}

void UpdateProfileName(Profile* profile,
                       const base::string16& new_profile_name) {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    return;
  }

  base::string16 current_profile_name =
      ProfileAttributesEntry::ShouldConcatenateGaiaAndProfileName()
          ? entry->GetLocalProfileName()
          : entry->GetName();

  if (new_profile_name == current_profile_name)
    return;

  // This is only called when updating the profile name through the UI,
  // so we can assume the user has done this on purpose.
  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetBoolean(prefs::kProfileUsingDefaultName, false);

  // Updating the profile preference will cause the profile attributes storage
  // to be updated for this preference.
  pref_service->SetString(prefs::kProfileName,
                          base::UTF16ToUTF8(new_profile_name));
}

#endif  // !defined(OS_CHROMEOS)

bool IsRegularOrGuestSession(Browser* browser) {
  Profile* profile = browser->profile();
  return profile->IsRegularProfile() || profile->IsGuestSession();
}

bool IsProfileLocked(const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_path, &entry)) {
    return false;
  }

  return entry->IsSigninRequired();
}

#if !defined(OS_CHROMEOS)
void UpdateIsProfileLockEnabledIfNeeded(Profile* profile) {
  if (!profile->GetPrefs()->GetString(prefs::kGoogleServicesHostedDomain).
      empty())
    return;

  UpdateGaiaProfileInfoIfNeeded(profile);
}

void UpdateGaiaProfileInfoIfNeeded(Profile* profile) {
  DCHECK(profile);

  GAIAInfoUpdateService* service =
      GAIAInfoUpdateServiceFactory::GetInstance()->GetForProfile(profile);
  // The service may be null, for example during unit tests.
  if (service)
    service->Update();
}

SigninErrorController* GetSigninErrorController(Profile* profile) {
  return SigninErrorControllerFactory::GetForProfile(profile);
}

bool SetActiveProfileToGuestIfLocked() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  const base::FilePath& active_profile_path =
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir());
  const base::FilePath& guest_path = ProfileManager::GetGuestProfilePath();
  if (active_profile_path == guest_path)
    return true;

  ProfileAttributesEntry* entry;
  bool has_entry =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(active_profile_path, &entry);

  // |has_entry| may be false if a profile is specified on the command line.
  if (has_entry && !entry->IsSigninRequired())
    return false;

  SetLastUsedProfile(guest_path.BaseName().MaybeAsASCII());

  return true;
}
#endif  // !defined(OS_CHROMEOS)

void RemoveBrowsingDataForProfile(const base::FilePath& profile_path) {
  // The BrowsingDataRemover relies on many objects that aren't created in unit
  // tests. Previously this code would depend on content::ResourceDispatcherHost
  // but that's gone, so do a similar hack for now.
  if (!g_browser_process->safe_browsing_service())
    return;

  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
      profile_path);
  if (!profile)
    return;

  // For guest profiles the browsing data is in the OTR profile.
  if (profile->IsGuestSession())
    profile = profile->GetOffTheRecordProfile();

  profile->Wipe();
}

#if !defined(OS_CHROMEOS)
bool AreAllNonChildNonSupervisedProfilesLocked() {
  bool at_least_one_regular_profile_present = false;

  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetAllProfilesAttributes();
  for (const ProfileAttributesEntry* entry : entries) {
    if (entry->IsOmitted())
      continue;

    // Only consider non-child and non-supervised profiles.
    if (!entry->IsChild() && !entry->IsLegacySupervised()) {
      at_least_one_regular_profile_present = true;

      if (!entry->IsSigninRequired())
        return false;
    }
  }
  return at_least_one_regular_profile_present;
}
#endif

bool IsPublicSession() {
#if defined(OS_CHROMEOS)
  if (chromeos::LoginState::IsInitialized()) {
    return chromeos::LoginState::Get()->IsPublicSessionUser();
  }
#endif
  return false;
}

bool ArePublicSessionRestrictionsEnabled() {
#if defined(OS_CHROMEOS)
  if (chromeos::LoginState::IsInitialized()) {
    return chromeos::LoginState::Get()->ArePublicSessionRestrictionsEnabled();
  }
#endif
  return false;
}
#endif  // !defined(OS_ANDROID)

}  // namespace profiles

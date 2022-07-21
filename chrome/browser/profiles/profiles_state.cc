// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browsing_data_remover.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chromeos/login/login_state/login_state.h"
#else
#include <algorithm>
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "components/signin/public/base/signin_pref_names.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace profiles {

bool IsMultipleProfilesEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
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
  registry->RegisterBooleanPref(prefs::kBrowserShowProfilePickerOnStartup,
                                true);
  registry->RegisterIntegerPref(
      prefs::kBrowserProfilePickerAvailabilityOnStartup,
      static_cast<int>(ProfilePicker::AvailabilityOnStartup::kEnabled));
  registry->RegisterBooleanPref(prefs::kBrowserProfilePickerShown, false);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kLacrosSecondaryProfilesAllowed, true);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void SetLastUsedProfile(const base::FilePath& profile_dir) {
  // We should never be saving the System Profile as the last one used since it
  // shouldn't have a browser.
  if (profile_dir == base::FilePath(chrome::kSystemProfileDir))
    return;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->SetFilePath(prefs::kProfileLastUsed, profile_dir);
}

#if !BUILDFLAG(IS_ANDROID)
std::u16string GetAvatarNameForProfile(const base::FilePath& profile_path) {
  if (profile_path == ProfileManager::GetGuestProfilePath()) {
    return l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  }

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();

  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile_path);
  if (!entry)
    return l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);

  const std::u16string profile_name_to_display = entry->GetName();
  // If the user has set their local profile name on purpose.
  bool is_default_name = entry->IsUsingDefaultName();
  if (!is_default_name)
    return profile_name_to_display;

  // The profile is signed in and has a GAIA name.
  const std::u16string gaia_name_to_display = entry->GetGAIANameToDisplay();
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
  const std::u16string email = entry->GetUserName();
  return email.empty() ? profile_name_to_display : email;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void UpdateProfileName(Profile* profile,
                       const std::u16string& new_profile_name) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    return;
  }

  if (new_profile_name == entry->GetLocalProfileName())
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

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

bool IsRegularOrGuestSession(Browser* browser) {
  Profile* profile = browser->profile();
  return profile->IsRegularProfile() || profile->IsGuestSession();
}

bool IsGuestModeRequested(const base::CommandLine& command_line,
                          PrefService* local_state,
                          bool show_warning) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
  DCHECK(local_state);

  // Check if guest mode enforcement commandline switch or policy are provided.
  if (command_line.HasSwitch(switches::kGuest) ||
      local_state->GetBoolean(prefs::kBrowserGuestModeEnforced)) {
    // Check if guest mode is allowed by policy.
    if (local_state->GetBoolean(prefs::kBrowserGuestModeEnabled))
      return true;
    if (show_warning) {
      LOG(WARNING) << "Guest mode disabled by policy, launching a normal "
                   << "browser session.";
    }
  }
#endif
  return false;
}

bool IsProfileCreationAllowed() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!AreSecondaryProfilesAllowed())
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const PrefService* const pref_service = g_browser_process->local_state();
  DCHECK(pref_service);
  return pref_service->GetBoolean(prefs::kBrowserAddPersonEnabled);
}

bool IsGuestModeEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  if (!AreSecondaryProfilesAllowed())
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  const PrefService* const pref_service = g_browser_process->local_state();
  DCHECK(pref_service);
  return pref_service->GetBoolean(prefs::kBrowserGuestModeEnabled);
}

#if BUILDFLAG(IS_CHROMEOS)
bool AreSecondaryProfilesAllowed() {
  const PrefService* const pref_service = g_browser_process->local_state();
  DCHECK(pref_service);
  // This Lacros policy is used on Ash, as it impacts the Ash UI where the user
  // can launch Lacros Guest mode window.
  return pref_service->GetBoolean(prefs::kLacrosSecondaryProfilesAllowed);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsProfileLocked(const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    return false;
  }

  return entry->IsSigninRequired();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void UpdateGaiaProfileInfoIfNeeded(Profile* profile) {
  DCHECK(profile);

  GAIAInfoUpdateService* service =
      GAIAInfoUpdateServiceFactory::GetInstance()->GetForProfile(profile);
  // The service may be null, for example during unit tests.
  if (service)
    service->UpdatePrimaryAccount();
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void RemoveBrowsingDataForProfile(const base::FilePath& profile_path) {
  // The BrowsingDataRemover relies on many objects that aren't created in unit
  // tests. Previously this code would depend on content::ResourceDispatcherHost
  // but that's gone, so do a similar hack for now.
  if (!g_browser_process->safe_browsing_service())
    return;

  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (!profile)
    return;

  // For guest profiles the browsing data is in the OTR profile.
  if (profile->IsGuestSession())
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  profile->Wipe();
}

bool IsPublicSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return chromeos::LoginState::IsInitialized() &&
         chromeos::LoginState::Get()->IsPublicSessionUser();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->SessionType() ==
         crosapi::mojom::SessionType::kPublicSession;
#else
  return false;
#endif
}

bool ArePublicSessionRestrictionsEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::LoginState::IsInitialized()) {
    return chromeos::LoginState::Get()->ArePublicSessionRestrictionsEnabled();
  }
#endif
  return false;
}

bool IsKioskSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return chromeos::LoginState::IsInitialized() &&
         chromeos::LoginState::Get()->IsKioskSession();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::SessionType session_type =
      chromeos::BrowserParamsProxy::Get()->SessionType();
  return session_type == crosapi::mojom::SessionType::kWebKioskSession ||
         session_type == crosapi::mojom::SessionType::kAppKioskSession;
#else
  return false;
#endif
}

bool IsChromeAppKioskSession() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::UserManager::Get()->IsLoggedInAsKioskApp();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::SessionType session_type =
      chromeos::BrowserParamsProxy::Get()->SessionType();
  return session_type == crosapi::mojom::SessionType::kAppKioskSession;
#else
  return false;
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsWebKioskSession() {
  crosapi::mojom::SessionType session_type =
      chromeos::BrowserParamsProxy::Get()->SessionType();
  return session_type == crosapi::mojom::SessionType::kWebKioskSession;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Implemented to have the same logic as user_manager::User::HasGaiaAccount()
bool SessionHasGaiaAccount() {
  crosapi::mojom::SessionType session_type =
      chromeos::BrowserParamsProxy::Get()->SessionType();
  return session_type == crosapi::mojom::SessionType::kRegularSession ||
         session_type == crosapi::mojom::SessionType::kChildSession;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string GetDefaultNameForNewEnterpriseProfile(
    const std::string& hosted_domain) {
  if (AccountInfo::IsManaged(hosted_domain))
    return base::UTF8ToUTF16(hosted_domain);
  return l10n_util::GetStringUTF16(
      IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_PROFILE_NAME);
}

std::u16string GetDefaultNameForNewSignedInProfile(
    const AccountInfo& account_info) {
  DCHECK(account_info.IsValid());
  if (!account_info.IsManaged())
    return base::UTF8ToUTF16(account_info.given_name);
  return GetDefaultNameForNewEnterpriseProfile(account_info.hosted_domain);
}

std::u16string GetDefaultNameForNewSignedInProfileWithIncompleteInfo(
    const CoreAccountInfo& account_info) {
  // As a fallback, use the email of the user as the profile name when extended
  // account info is not available.
  return base::UTF8ToUTF16(account_info.email);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace profiles

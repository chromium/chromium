// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
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
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/common/features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browsing_data_remover.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#else
#include <algorithm>
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "components/signin/public/base/signin_pref_names.h"
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
#elif !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kEnterpriseProfileCreationKeepBrowsingData, false);
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

bool IsRegularUserProfile(Profile* profile) {
  ProfileSelections selections =
      ProfileSelections::Builder()
          .WithRegular(ProfileSelection::kOriginalOnly)  // the default
          // Filter out ChromeOS irregular profiles (login, lock screen...);
          // they are of type kRegular (returns true for `Profile::IsRegular()`)
          // but aren't used to browse the web and users can't configure them.
          .WithAshInternals(ProfileSelection::kNone)
          .Build();
  return selections.ApplyProfileSelection(profile);
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

#if !BUILDFLAG(IS_CHROMEOS)
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

#endif  // !BUILDFLAG(IS_CHROMEOS)

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
  const PrefService* const pref_service = g_browser_process->local_state();
  DCHECK(pref_service);
  return pref_service->GetBoolean(prefs::kBrowserAddPersonEnabled);
}

// Whether guest mode is globally disabled (for all entry points and users).
bool IsGuestModeGloballyDisabledInternal() {
#if BUILDFLAG(IS_CHROMEOS)
  if (!AreSecondaryProfilesAllowed()) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  const PrefService* const pref_service = g_browser_process->local_state();
  DCHECK(pref_service);
  return !pref_service->GetBoolean(prefs::kBrowserGuestModeEnabled);
}

bool IsGuestModeEnabled() {
  if (IsGuestModeGloballyDisabledInternal()) {
    return false;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // If there are any supervised profiles, disable guest mode.
  if (base::FeatureList::IsEnabled(
          supervised_user::kHideGuestModeForSupervisedUsers) &&
      base::ranges::any_of(g_browser_process->profile_manager()
                               ->GetProfileAttributesStorage()
                               .GetAllProfilesAttributes(),
                           [](const ProfileAttributesEntry* entry) {
                             return entry->IsSupervised() &&
                                    !entry->IsOmitted();
                           })) {
    return false;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  return true;
}

bool IsGuestModeEnabled(const Profile& profile) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          supervised_user::kHideGuestModeForSupervisedUsers)) {
    ProfileAttributesEntry* profile_attributes =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile.GetPath());
    if (profile_attributes && profile_attributes->IsSupervised()) {
      return false;
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  return !IsGuestModeGloballyDisabledInternal();
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

#if !BUILDFLAG(IS_CHROMEOS)
void UpdateGaiaProfileInfoIfNeeded(Profile* profile) {
  DCHECK(profile);

  GAIAInfoUpdateService* service =
      GAIAInfoUpdateServiceFactory::GetInstance()->GetForProfile(profile);
  // The service may be null, for example during unit tests.
  if (service)
    service->UpdatePrimaryAccount();
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

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

bool IsDemoSession() {
#if BUILDFLAG(IS_CHROMEOS)
  return ash::DemoSession::IsDeviceInDemoMode();
#else
  return false;
#endif
}

bool IsChromeAppKioskSession() {
#if BUILDFLAG(IS_CHROMEOS)
  return user_manager::UserManager::Get()->IsLoggedInAsKioskApp();
#else
  return false;
#endif
}

#if !BUILDFLAG(IS_CHROMEOS)
std::u16string GetDefaultNameForNewEnterpriseProfile(
    const std::string& hosted_domain) {
  if (AccountInfo::IsManaged(hosted_domain)) {
    std::u16string hosted_domain_name = base::UTF8ToUTF16(hosted_domain);
    CHECK(!hosted_domain_name.empty());
    return hosted_domain_name;
  }
  std::u16string default_name = l10n_util::GetStringUTF16(
      IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_PROFILE_NAME);
  CHECK(!default_name.empty());
  return default_name;
}

std::u16string GetDefaultNameForNewSignedInProfile(
    const AccountInfo& account_info) {
  DCHECK(account_info.IsValid());
  if (!account_info.IsManaged()) {
    std::u16string given_name = base::UTF8ToUTF16(account_info.given_name);
    CHECK(!given_name.empty());
    return given_name;
  }
  std::u16string default_name =
      GetDefaultNameForNewEnterpriseProfile(account_info.hosted_domain);
  CHECK(!default_name.empty());
  return default_name;
}

std::u16string GetDefaultNameForNewSignedInProfileWithIncompleteInfo(
    const CoreAccountInfo& account_info) {
  // As a fallback, use the email of the user as the profile name when extended
  // account info is not available.
  CHECK(!account_info.email.empty());
  return base::UTF8ToUTF16(account_info.email);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace profiles

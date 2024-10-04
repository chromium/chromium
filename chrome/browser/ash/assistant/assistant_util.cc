// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/assistant/assistant_util.h"

#include <string>

#include "ash/constants/devicetype.h"
#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace {

using ::ash::assistant::AssistantAllowedState;

bool g_override_is_google_device = false;

bool HasPrimaryAccount(const Profile* profile) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  if (!identity_manager)
    return false;

  return identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

bool IsGoogleDevice() {
  return g_override_is_google_device || ash::IsGoogleBrandedDevice();
}

const user_manager::User* GetUser(const Profile* profile) {
  return ash::ProfileHelper::Get()->GetUserByProfile(profile);
}

bool IsAssistantAllowedForUserType(const Profile* profile) {
  return GetUser(profile)->HasGaiaAccount();
}

// Get the actual reason why the user type is not allowed.
AssistantAllowedState GetErrorForUserType(const Profile* profile) {
  DCHECK(!IsAssistantAllowedForUserType(profile));
  switch (GetUser(profile)->GetType()) {
    case user_manager::UserType::kPublicAccount:
      return AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION;

    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE;

    case user_manager::UserType::kGuest:
      return AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE;

    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      // This method should only be called for disallowed user types.
      NOTREACHED_IN_MIGRATION();
      return AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE;
  }
}

bool IsAssistantAllowedForLocale(const Profile* profile) {
  // String literals used in some cases in the array because their
  // constant equivalents don't exist in:
  // third_party/icu/source/common/unicode/uloc.h
  const std::string kAllowedLocales[] = {ULOC_CANADA,  ULOC_CANADA_FRENCH,
                                         ULOC_FRANCE,  ULOC_FRENCH,
                                         ULOC_GERMANY, ULOC_ITALY,
                                         ULOC_JAPAN,   ULOC_JAPANESE,
                                         ULOC_UK,      ULOC_US,
                                         "da",         "en_AU",
                                         "en_IN",      "en_NZ",
                                         "es_CO",      "es_ES",
                                         "es_MX",      "fr_BE",
                                         "it",         "nb",
                                         "nl",         "nn",
                                         "no",         "sv"};

  const PrefService* prefs = profile->GetPrefs();
  std::string pref_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  // Also accept runtime locale which maybe an approximation of user's pref
  // locale.
  const std::string kRuntimeLocale = icu::Locale::getDefault().getName();

  base::ReplaceChars(pref_locale, "-", "_", &pref_locale);
  bool allowed = base::Contains(kAllowedLocales, pref_locale) ||
                 base::Contains(kAllowedLocales, kRuntimeLocale);

  return allowed;
}

bool IsAssistantDisabledByPolicy(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      ash::assistant::prefs::kAssistantDisabledByPolicy);
}

bool IsEmailDomainSupported(const Profile* profile) {
  const std::string email = GetUser(profile)->GetAccountId().GetUserEmail();
  DCHECK(!email.empty());

  return (gaia::ExtractDomainName(email) == "gmail.com" ||
          gaia::ExtractDomainName(email) == "googlemail.com" ||
          gaia::IsGoogleInternalAccountEmail(email));
}

bool HasDedicatedAssistantKey() {
  return IsGoogleDevice();
}

}  // namespace

namespace assistant {

AssistantAllowedState IsAssistantAllowedForProfile(const Profile* profile) {
  // Disabled because the libassistant.so is not available.
  if (!ash::assistant::features::IsLibAssistantDLCEnabled()) {
    return AssistantAllowedState::DISALLOWED_BY_NO_BINARY;
  }

  // Primary account might be missing during unittests.
  if (!HasPrimaryAccount(profile))
    return AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER;

  if (!ash::ProfileHelper::IsPrimaryProfile(profile))
    return AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER;

  if (profile->IsOffTheRecord())
    return AssistantAllowedState::DISALLOWED_BY_INCOGNITO;

  if (ash::DemoSession::IsDeviceInDemoMode())
    return AssistantAllowedState::DISALLOWED_BY_DEMO_MODE;

  if (!IsAssistantAllowedForUserType(profile))
    return GetErrorForUserType(profile);

  if (!IsAssistantAllowedForLocale(profile))
    return AssistantAllowedState::DISALLOWED_BY_LOCALE;

  if (IsAssistantDisabledByPolicy(profile))
    return AssistantAllowedState::DISALLOWED_BY_POLICY;

  // Bypass the email domain check when the account is logged in a device with
  // dedicated Assistant key.
  if (!HasDedicatedAssistantKey() && !IsEmailDomainSupported(profile))
    return AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE;

  return AssistantAllowedState::ALLOWED;
}

void OverrideIsGoogleDeviceForTesting(bool is_google_device) {
  g_override_is_google_device = is_google_device;
}

}  // namespace assistant

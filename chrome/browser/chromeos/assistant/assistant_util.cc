// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/assistant/assistant_util.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/chromeos/events/keyboard_layout_util.h"

namespace assistant {

ash::mojom::AssistantAllowedState IsAssistantAllowedForProfile(
    const Profile* profile) {
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER;

  if (profile->IsOffTheRecord())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_INCOGNITO;

  if (profile->IsLegacySupervised())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_SUPERVISED_USER;

  if (chromeos::DemoSession::IsDeviceInDemoMode())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_DEMO_MODE;

  if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount())
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION;

  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE;
  }

  // String literals used in some cases in the array because their
  // constant equivalents don't exist in:
  // third_party/icu/source/common/unicode/uloc.h
  const std::string kAllowedLocales[] = {ULOC_CANADA,
                                         ULOC_CANADA_FRENCH,
                                         ULOC_FRANCE,
                                         ULOC_FRENCH,
                                         ULOC_GERMANY,
                                         ULOC_ITALY,
                                         ULOC_JAPAN,
                                         ULOC_JAPANESE,
                                         ULOC_UK,
                                         ULOC_US,
                                         "da",
                                         "en_AU",
                                         "en_IN",
                                         "en_NZ",
                                         "es_CO",
                                         "es_ES",
                                         "es_MX",
                                         "fr_BE",
                                         "nb",
                                         "nl",
                                         "nn",
                                         "no",
                                         "sv"};

  const PrefService* prefs = profile->GetPrefs();
  std::string pref_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  // Also accept runtime locale which maybe an approximation of user's pref
  // locale.
  const std::string kRuntimeLocale = icu::Locale::getDefault().getName();
  // Bypass locale check when using fake gaia login. There is no need to enforce
  // in these test environments.
  if (!chromeos::switches::IsGaiaServicesDisabled() && !pref_locale.empty()) {
    base::ReplaceChars(pref_locale, "-", "_", &pref_locale);
    bool disallowed = !base::Contains(kAllowedLocales, pref_locale) &&
                      !base::Contains(kAllowedLocales, kRuntimeLocale);

    if (disallowed)
      return ash::mojom::AssistantAllowedState::DISALLOWED_BY_LOCALE;
  }

  if (prefs->GetBoolean(chromeos::assistant::prefs::kAssistantDisabledByPolicy))
    return ash::mojom::AssistantAllowedState::DISALLOWED_BY_POLICY;

  // Bypass the account type check when using fake gaia login, e.g. in Tast
  // tests, or the account is logged in a device with a physical Assistant key
  // on keyboard.
  if (!chromeos::switches::IsGaiaServicesDisabled() &&
      !(ui::DeviceKeyboardHasAssistantKey() ||
        base::EqualsCaseInsensitiveASCII(base::SysInfo::GetLsbReleaseBoard(),
                                         "nocturne"))) {
    // Only enable non-dasher accounts for devices without physical key.
    bool account_supported = false;
    auto* identity_manager =
        IdentityManagerFactory::GetForProfileIfExists(profile);

    if (identity_manager) {
      const std::string email = identity_manager->GetPrimaryAccountInfo().email;
      if (!email.empty() &&
          (gaia::ExtractDomainName(email) == "gmail.com" ||
           gaia::ExtractDomainName(email) == "googlemail.com" ||
           gaia::IsGoogleInternalAccountEmail(email))) {
        account_supported = true;
      }
    }

    if (!account_supported)
      return ash::mojom::AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE;
  }

  return ash::mojom::AssistantAllowedState::ALLOWED;
}

}  // namespace assistant

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_availability.h"

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/user_manager/user_manager.h"
#include "components/variations/service/variations_service.h"

namespace ash::mahi_availability {

bool CanUseMahiService() {
  if (!manta::features::IsMantaServiceEnabled()) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kMahiRestrictionsOverride)) {
    return true;
  }

  if (!ash::DemoSession::IsDeviceInDemoMode()) {
    if (!user_manager::UserManager::IsInitialized() ||
        !user_manager::UserManager::Get()->IsUserLoggedIn()) {
      return false;
    }

    Profile* profile = ProfileManager::GetActiveUserProfile();
    if (!profile) {
      return false;
    }

    // Controls for managed users.
    if (profile->GetProfilePolicyConnector()->IsManaged() &&
        !chromeos::features::IsMahiManagedEnabled()) {
      return false;
    }

    // Guest session is not allowed when not in demo mode.
    if (profile->IsGuestSession()) {
      return false;
    }

    // MantaService might not be available in tests.
    if (manta::MantaService* service =
            manta::MantaServiceFactory::GetForProfile(profile);
        service && service->CanAccessMantaFeaturesWithoutMinorRestrictions() !=
                       manta::FeatureSupportStatus::kSupported) {
      return false;
    }
  }

  const std::string country_code =
      (g_browser_process != nullptr &&
       g_browser_process->variations_service() != nullptr)
          ? g_browser_process->variations_service()->GetLatestCountry()
          : std::string();
  static constexpr auto kCountryAllowlist =
      base::MakeFixedFlatSet<std::string_view>({
          "ad", "ae", "ag", "ai", "al", "am", "ao", "aq", "ar", "as", "at",
          "au", "aw", "ax", "az", "ba", "bb", "bd", "be", "bf", "bg", "bh",
          "bi", "bj", "bl", "bm", "bn", "bo", "bq", "br", "bs", "bt", "bw",
          "bz", "ca", "cc", "cf", "cg", "ch", "ci", "ck", "cl", "cm", "co",
          "cr", "cv", "cw", "cx", "cy", "cz", "de", "dj", "dk", "dm", "do",
          "dz", "ec", "ee", "eg", "eh", "er", "es", "et", "fi", "fj", "fk",
          "fm", "fo", "fr", "ga", "gb", "gd", "ge", "gf", "gg", "gh", "gi",
          "gl", "gm", "gn", "gp", "gq", "gr", "gs", "gt", "gu", "gw", "gy",
          "hm", "hn", "hr", "ht", "hu", "id", "ie", "il", "im", "in", "io",
          "iq", "is", "it", "je", "jm", "jo", "jp", "ke", "kg", "kh", "ki",
          "km", "kn", "kr", "kw", "ky", "kz", "la", "lb", "lc", "li", "lk",
          "lr", "ls", "lt", "lu", "lv", "ly", "ma", "mc", "md", "me", "mf",
          "mg", "mh", "mk", "ml", "mm", "mn", "mp", "mq", "mr", "ms", "mt",
          "mu", "mv", "mw", "mx", "my", "mz", "na", "nc", "ne", "nf", "ng",
          "ni", "nl", "no", "np", "nr", "nu", "nz", "om", "pa", "pe", "pf",
          "pg", "ph", "pk", "pl", "pm", "pn", "pr", "ps", "pt", "pw", "py",
          "qa", "re", "ro", "rs", "rw", "sa", "sb", "sc", "sd", "se", "sg",
          "sh", "si", "sj", "sk", "sl", "sm", "sn", "so", "sr", "ss", "st",
          "sv", "sx", "sz", "tc", "td", "tf", "tg", "th", "tj", "tk", "tl",
          "tm", "tn", "to", "tr", "tt", "tv", "tw", "tz", "ua", "ug", "um",
          "us", "uy", "uz", "va", "vc", "ve", "vg", "vi", "vn", "vu", "wf",
          "ws", "xk", "ye", "yt", "za", "zm", "zr", "zw",
      });

  return kCountryAllowlist.contains(country_code);
}

bool IsMahiAvailable() {
  return chromeos::features::IsMahiEnabled() && CanUseMahiService();
}

}  // namespace ash::mahi_availability

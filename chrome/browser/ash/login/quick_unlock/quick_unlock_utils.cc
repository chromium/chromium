// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace chromeos {
namespace quick_unlock {

namespace {
// Quick unlock is enabled regardless of flags.
bool enable_for_testing_ = false;
bool disable_pin_by_policy_for_testing_ = false;

// Options for the quick unlock allowlist.
const char kQuickUnlockAllowlistOptionAll[] = "all";
const char kQuickUnlockAllowlistOptionPin[] = "PIN";
const char kQuickUnlockAllowlistOptionFingerprint[] = "FINGERPRINT";

// Default minimum PIN length. Policy can increase or decrease this value.
constexpr int kDefaultMinimumPinLength = 6;

bool HasPolicyValue(const PrefService* pref_service, const char* value) {
  const base::ListValue* quick_unlock_allowlist =
      pref_service->GetList(prefs::kQuickUnlockModeAllowlist);
  return quick_unlock_allowlist->Find(base::Value(value)) !=
         quick_unlock_allowlist->end();
}

}  // namespace

bool IsFingerprintDisabledByPolicy(const PrefService* pref_service) {
  const bool enabled =
      HasPolicyValue(pref_service, kQuickUnlockAllowlistOptionAll) ||
      HasPolicyValue(pref_service, kQuickUnlockAllowlistOptionFingerprint);
  return !enabled;
}

base::TimeDelta PasswordConfirmationFrequencyToTimeDelta(
    PasswordConfirmationFrequency frequency) {
  switch (frequency) {
    case PasswordConfirmationFrequency::SIX_HOURS:
      return base::TimeDelta::FromHours(6);
    case PasswordConfirmationFrequency::TWELVE_HOURS:
      return base::TimeDelta::FromHours(12);
    case PasswordConfirmationFrequency::TWO_DAYS:
      return base::TimeDelta::FromDays(2);
    case PasswordConfirmationFrequency::WEEK:
      return base::TimeDelta::FromDays(7);
  }
  NOTREACHED();
  return base::TimeDelta();
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  base::Value::ListStorage quick_unlock_allowlist_default;
  quick_unlock_allowlist_default.emplace_back(kQuickUnlockAllowlistOptionAll);
  registry->RegisterListPref(
      prefs::kQuickUnlockModeAllowlist,
      base::Value(std::move(quick_unlock_allowlist_default)));
  registry->RegisterIntegerPref(
      prefs::kQuickUnlockTimeout,
      static_cast<int>(PasswordConfirmationFrequency::TWO_DAYS));

  // Preferences related the lock screen pin unlock.
  registry->RegisterIntegerPref(prefs::kPinUnlockMinimumLength,
                                kDefaultMinimumPinLength);
  // 0 indicates no maximum length for the pin.
  registry->RegisterIntegerPref(prefs::kPinUnlockMaximumLength, 0);
  registry->RegisterBooleanPref(prefs::kPinUnlockWeakPinsAllowed, true);

  // Register as true by default only when the feature is enabled.
  registry->RegisterBooleanPref(
      prefs::kPinUnlockAutosubmitEnabled,
      features::IsPinAutosubmitFeatureEnabled());
}

bool IsPinDisabledByPolicy(PrefService* pref_service) {
  if (disable_pin_by_policy_for_testing_)
    return true;

  if (enable_for_testing_)
    return false;

  const bool enabled =
      HasPolicyValue(pref_service, kQuickUnlockAllowlistOptionAll) ||
      HasPolicyValue(pref_service, kQuickUnlockAllowlistOptionPin);
  return !enabled;
}

bool IsPinEnabled(PrefService* pref_service) {
  if (enable_for_testing_)
    return true;

  // PIN is disabled for deprecated supervised user, but allowed to child user.
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user && user->GetType() ==
                  user_manager::UserType::USER_TYPE_SUPERVISED_DEPRECATED) {
    return false;
  }

  return true;
}

// Returns fingerprint location depending on the commandline switch.
// TODO(rsorokin): Add browser tests for different assets.
FingerprintLocation GetFingerprintLocation() {
  const FingerprintLocation default_location = FingerprintLocation::UNKNOWN;
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kFingerprintSensorLocation))
    return default_location;

  const std::string location_info =
      cl->GetSwitchValueASCII(switches::kFingerprintSensorLocation);
  if (location_info == "power-button-top-left")
    return FingerprintLocation::TABLET_POWER_BUTTON;
  if (location_info == "keyboard-bottom-left")
    return FingerprintLocation::KEYBOARD_BOTTOM_LEFT;
  if (location_info == "keyboard-bottom-right")
    return FingerprintLocation::KEYBOARD_BOTTOM_RIGHT;
  if (location_info == "keyboard-top-right")
    return FingerprintLocation::KEYBOARD_TOP_RIGHT;
  if (location_info == "right-side")
    return FingerprintLocation::RIGHT_SIDE;
  if (location_info == "left-side")
    return FingerprintLocation::LEFT_SIDE;
  NOTREACHED() << "Not handled value: " << location_info;
  return default_location;
}

bool IsFingerprintSupported() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return base::FeatureList::IsEnabled(::features::kQuickUnlockFingerprint) &&
         command_line->HasSwitch(switches::kFingerprintSensorLocation);
}

bool IsFingerprintEnabled(Profile* profile) {
  if (enable_for_testing_)
    return true;

  if (!IsFingerprintSupported())
    return false;

  // Disable fingerprint if the profile does not belong to the primary user.
  if (profile != ProfileManager::GetPrimaryUserProfile())
    return false;

  // Disable fingerprint if disallowed by policy.
  if (IsFingerprintDisabledByPolicy(profile->GetPrefs()))
    return false;

  return true;
}

void AddFingerprintResources(content::WebUIDataSource* html_source) {
  int resource_id;
  bool is_lottie_animation = false;
  switch (GetFingerprintLocation()) {
    case FingerprintLocation::TABLET_POWER_BUTTON:
      is_lottie_animation = true;
      resource_id = IDR_FINGERPRINT_TABLET_ANIMATION;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
      is_lottie_animation = true;
      resource_id = IDR_FINGERPRINT_LAPTOP_BOTTOM_RIGHT_ANIMATION;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_LEFT:
      resource_id = IDR_FINGERPRINT_LAPTOP_BOTTOM_LEFT_ILLUSTRATION_SVG;
      break;
    case FingerprintLocation::KEYBOARD_TOP_RIGHT:
      resource_id = IDR_FINGERPRINT_LAPTOP_TOP_RIGHT_ILLUSTRATION_SVG;
      break;
    case FingerprintLocation::RIGHT_SIDE:
    case FingerprintLocation::LEFT_SIDE:
    case FingerprintLocation::UNKNOWN:
      is_lottie_animation = true;
      resource_id = IDR_FINGERPRINT_DEFAULT_ANIMATION;
      break;
  }
  if (is_lottie_animation) {
    html_source->AddResourcePath("fingerprint_scanner_animation.json",
                                 resource_id);

    // To use lottie, the worker-src CSP needs to be updated for the web ui
    // that is using it. Since as of now there are only a couple of webuis
    // using lottie animations, this update has to be performed manually. As
    // the usage increases, set this as the default so manual override is no
    // longer required.
    html_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::WorkerSrc,
        "worker-src blob: 'self';");
  } else {
    html_source->AddResourcePath("fingerprint_scanner_illustration.svg",
                                 resource_id);
  }
  html_source->AddBoolean("useLottieAnimationForFingerprint",
                          is_lottie_animation);
}

void EnabledForTesting(bool state) {
  enable_for_testing_ = state;
}

void DisablePinByPolicyForTesting(bool disable) {
  disable_pin_by_policy_for_testing_ = disable;
}

}  // namespace quick_unlock
}  // namespace chromeos

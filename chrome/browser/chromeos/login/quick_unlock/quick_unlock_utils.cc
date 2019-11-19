// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace quick_unlock {

namespace {
// Quick unlock is enabled regardless of flags.
bool enable_for_testing_ = false;
bool disable_pin_by_policy_for_testing_ = false;

// Options for the quick unlock whitelist.
const char kQuickUnlockWhitelistOptionAll[] = "all";
const char kQuickUnlockWhitelistOptionPin[] = "PIN";
const char kQuickUnlockWhitelistOptionFingerprint[] = "FINGERPRINT";

// Default minimum PIN length. Policy can increase or decrease this value.
constexpr int kDefaultMinimumPinLength = 6;

bool HasPolicyValue(const PrefService* pref_service, const char* value) {
  const base::ListValue* quick_unlock_whitelist =
      pref_service->GetList(prefs::kQuickUnlockModeWhitelist);
  return quick_unlock_whitelist->Find(base::Value(value)) !=
         quick_unlock_whitelist->end();
}

bool IsFingerprintDisabledByPolicy(const PrefService* pref_service) {
  const bool enabled =
      HasPolicyValue(pref_service, kQuickUnlockWhitelistOptionAll) ||
      HasPolicyValue(pref_service, kQuickUnlockWhitelistOptionFingerprint);
  return !enabled;
}

}  // namespace

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
  base::Value::ListStorage quick_unlock_whitelist_default;
  quick_unlock_whitelist_default.emplace_back(kQuickUnlockWhitelistOptionAll);
  registry->RegisterListPref(
      prefs::kQuickUnlockModeWhitelist,
      base::Value(std::move(quick_unlock_whitelist_default)));
  registry->RegisterIntegerPref(
      prefs::kQuickUnlockTimeout,
      static_cast<int>(PasswordConfirmationFrequency::TWO_DAYS));

  // Preferences related the lock screen pin unlock.
  registry->RegisterIntegerPref(prefs::kPinUnlockMinimumLength,
                                kDefaultMinimumPinLength);
  // 0 indicates no maximum length for the pin.
  registry->RegisterIntegerPref(prefs::kPinUnlockMaximumLength, 0);
  registry->RegisterBooleanPref(prefs::kPinUnlockWeakPinsAllowed, true);
}

bool IsPinDisabledByPolicy(PrefService* pref_service) {
  if (disable_pin_by_policy_for_testing_)
    return true;

  if (enable_for_testing_)
    return false;

  const bool enabled =
      HasPolicyValue(pref_service, kQuickUnlockWhitelistOptionAll) ||
      HasPolicyValue(pref_service, kQuickUnlockWhitelistOptionPin);
  return !enabled;
}

bool IsPinEnabled(PrefService* pref_service) {
  if (enable_for_testing_)
    return true;

  // PIN is disabled for legacy supervised user, but allowed to child user.
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user && user->GetType() == user_manager::UserType::USER_TYPE_SUPERVISED)
    return false;

  // Enable quick unlock only if the switch is present.
  return base::FeatureList::IsEnabled(features::kQuickUnlockPin);
}

// Returns fingerprint location depending on the commandline switch.
// TODO(rsorokin): Add browser tests for different assets.
FingerprintLocation GetFingerprintLocation() {
  const FingerprintLocation default_location =
      FingerprintLocation::TABLET_POWER_BUTTON;
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kFingerprintSensorLocation))
    return default_location;

  const std::string location_info =
      cl->GetSwitchValueASCII(switches::kFingerprintSensorLocation);
  if (location_info == "power-button-top-left")
    return FingerprintLocation::TABLET_POWER_BUTTON;
  if (location_info == "keyboard-bottom-right")
    return FingerprintLocation::KEYBOARD_BOTTOM_RIGHT;
  if (location_info == "keyboard-top-right")
    return FingerprintLocation::KEYBOARD_TOP_RIGHT;
  NOTREACHED() << "Not handled value: " << location_info;
  return default_location;
}

bool IsFingerprintEnabled(Profile* profile) {
  if (enable_for_testing_)
    return true;

  // Disable fingerprint if the device does not have a fingerprint reader.
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kFingerprintSensorLocation))
    return false;

  // Disable fingerprint if the profile does not belong to the primary user.
  if (profile != ProfileManager::GetPrimaryUserProfile())
    return false;

  // Disable fingerprint if disallowed by policy.
  if (IsFingerprintDisabledByPolicy(profile->GetPrefs()))
    return false;

  // Enable fingerprint unlock only if the switch is present.
  return base::FeatureList::IsEnabled(features::kQuickUnlockFingerprint);
}

void EnabledForTesting(bool state) {
  enable_for_testing_ = state;
}

void DisablePinByPolicyForTesting(bool disable) {
  disable_pin_by_policy_for_testing_ = disable;
}

}  // namespace quick_unlock
}  // namespace chromeos

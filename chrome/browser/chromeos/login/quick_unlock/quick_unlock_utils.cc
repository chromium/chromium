// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
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
    case PasswordConfirmationFrequency::DAY:
      return base::TimeDelta::FromDays(1);
    case PasswordConfirmationFrequency::WEEK:
      return base::TimeDelta::FromDays(7);
  }
  NOTREACHED();
  return base::TimeDelta();
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  base::ListValue quick_unlock_whitelist_default;
  quick_unlock_whitelist_default.AppendString(kQuickUnlockWhitelistOptionAll);
  registry->RegisterListPref(prefs::kQuickUnlockModeWhitelist,
                             quick_unlock_whitelist_default.CreateDeepCopy());
  registry->RegisterIntegerPref(
      prefs::kQuickUnlockTimeout,
      static_cast<int>(PasswordConfirmationFrequency::DAY));

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

  // TODO(jdufault): Disable PIN for supervised users until we allow the owner
  // to set the PIN. See crbug.com/632797.
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user && user->IsSupervised())
    return false;

  // Enable quick unlock only if the switch is present.
  return base::FeatureList::IsEnabled(features::kQuickUnlockPin);
}

bool IsFingerprintEnabled(Profile* profile) {
  if (enable_for_testing_)
    return true;

  // Disable fingerprint if the profile does not belong to the primary user.
  if (profile != ProfileManager::GetPrimaryUserProfile())
    return false;

  // Disable fingerprint if disallowed by policy.
  if (IsFingerprintDisabledByPolicy(profile->GetPrefs()))
    return false;

  // Enable fingerprint unlock only if the switch is present.
  return base::FeatureList::IsEnabled(features::kQuickUnlockFingerprint);
}

void EnableForTesting() {
  enable_for_testing_ = true;
}

void DisablePinByPolicyForTesting(bool disable) {
  disable_pin_by_policy_for_testing_ = disable;
}

}  // namespace quick_unlock
}  // namespace chromeos

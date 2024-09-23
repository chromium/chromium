// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace ash {
namespace quick_unlock {
namespace {

// Maps FingerprintLocation to FingerprintDescriptionStrings.
constexpr auto kFingerprintLocationToStringsMap = base::MakeFixedFlatMap<
    quick_unlock::FingerprintLocation,
    quick_unlock::FingerprintDescriptionStrings>(
    {{quick_unlock::FingerprintLocation::TABLET_POWER_BUTTON,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION_CHILD}},

     {quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_LEFT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_KEYBOARD_BOTTOM_LEFT_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_KEYBOARD_BOTTOM_LEFT_DESCRIPTION_CHILD}},

     {quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_RIGHT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_KEYBOARD_BOTTOM_RIGHT_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_KEYBOARD_BOTTOM_RIGHT_DESCRIPTION_CHILD}},

     {quick_unlock::FingerprintLocation::KEYBOARD_TOP_RIGHT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_KEYBOARD_TOP_RIGHT_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_KEYBOARD_TOP_RIGHT_DESCRIPTION_CHILD}},

     {quick_unlock::FingerprintLocation::RIGHT_SIDE,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_RIGHT_SIDE_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_RIGHT_SIDE_DESCRIPTION_CHILD}},

     {quick_unlock::FingerprintLocation::LEFT_SIDE,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_SIDE_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_SIDE_DESCRIPTION_CHILD}},

     {quick_unlock::FingerprintLocation::LEFT_OF_POWER_BUTTON_TOP_RIGHT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_OF_POWER_BUTTON_TOP_RIGHT_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_OF_POWER_BUTTON_TOP_RIGHT_DESCRIPTION_CHILD}},

     {quick_unlock::FingerprintLocation::UNKNOWN,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD}}});

TestApi* g_instance = nullptr;

// Options for the quick unlock allowlist.
const char kFactorsOptionAll[] = "all";
const char kFactorsOptionPin[] = "PIN";
const char kFactorsOptionFingerprint[] = "FINGERPRINT";

// Default minimum PIN length. Policy can increase or decrease this value.
constexpr int kDefaultMinimumPinLength = 6;

bool HasPolicyValue(const PrefService* pref_service,
                    Purpose purpose,
                    const char* value) {
  const base::Value::List* factors = nullptr;
  switch (purpose) {
    case Purpose::kUnlock:
      factors = &pref_service->GetList(prefs::kQuickUnlockModeAllowlist);
      break;
    case Purpose::kWebAuthn:
      factors = &pref_service->GetList(prefs::kWebAuthnFactors);
      break;
    default:
      return false;
  }
  return base::Contains(*factors, base::Value(value));
}

// Check if fingerprint is disabled for a specific purpose (so not including
// kAny) by reading the policy value.
bool IsFingerprintDisabledByPolicySinglePurpose(const PrefService* pref_service,
                                                Purpose purpose) {
  DCHECK(purpose != Purpose::kAny);
  const bool enabled =
      HasPolicyValue(pref_service, purpose, kFactorsOptionAll) ||
      HasPolicyValue(pref_service, purpose, kFactorsOptionFingerprint);
  return !enabled;
}

// Check if pin is disabled for a specific purpose (so not including
// kAny) by reading the policy value.
bool IsPinDisabledByPolicySinglePurpose(const PrefService* pref_service,
                                        Purpose purpose) {
  DCHECK(purpose != Purpose::kAny);
  const bool enabled =
      HasPolicyValue(pref_service, purpose, kFactorsOptionAll) ||
      HasPolicyValue(pref_service, purpose, kFactorsOptionPin);
  return !enabled;
}

}  // namespace

TestApi::TestApi(bool override_quick_unlock)
    : overridden_(override_quick_unlock) {
  old_instance_ = g_instance;
  g_instance = this;
  std::fill(pin_purposes_enabled_by_policy_,
            pin_purposes_enabled_by_policy_ + kNumOfPurposes, false);
  std::fill(fingerprint_purposes_enabled_by_policy_,
            fingerprint_purposes_enabled_by_policy_ + kNumOfPurposes, false);
}

TestApi::~TestApi() {
  CHECK_EQ(g_instance, this);
  g_instance = old_instance_;
}

TestApi* TestApi::Get() {
  return g_instance;
}

bool TestApi::IsQuickUnlockOverridden() {
  return overridden_;
}

void TestApi::EnablePinByPolicy(Purpose purpose) {
  if (purpose != Purpose::kAny) {
    pin_purposes_enabled_by_policy_[static_cast<int>(Purpose::kAny)] = true;
  }
  pin_purposes_enabled_by_policy_[static_cast<int>(purpose)] = true;
}

void TestApi::EnableFingerprintByPolicy(Purpose purpose) {
  if (purpose != Purpose::kAny) {
    fingerprint_purposes_enabled_by_policy_[static_cast<int>(Purpose::kAny)] =
        true;
  }
  fingerprint_purposes_enabled_by_policy_[static_cast<int>(purpose)] = true;
}

bool TestApi::IsPinEnabledByPolicy(Purpose purpose) {
  return pin_purposes_enabled_by_policy_[static_cast<int>(purpose)];
}

bool TestApi::IsFingerprintEnabledByPolicy(Purpose purpose) {
  return fingerprint_purposes_enabled_by_policy_[static_cast<int>(purpose)];
}

bool IsFingerprintDisabledByPolicy(const PrefService* pref_service,
                                   Purpose purpose) {
  auto* test_api = TestApi::Get();
  if (test_api && test_api->IsQuickUnlockOverridden()) {
    return !test_api->IsFingerprintEnabledByPolicy(purpose);
  }

  if (purpose == Purpose::kAny) {
    return IsFingerprintDisabledByPolicySinglePurpose(pref_service,
                                                      Purpose::kUnlock) &&
           IsFingerprintDisabledByPolicySinglePurpose(pref_service,
                                                      Purpose::kWebAuthn);
  }
  return IsFingerprintDisabledByPolicySinglePurpose(pref_service, purpose);
}

base::TimeDelta PasswordConfirmationFrequencyToTimeDelta(
    PasswordConfirmationFrequency frequency) {
  switch (frequency) {
    case PasswordConfirmationFrequency::SIX_HOURS:
      return base::Hours(6);
    case PasswordConfirmationFrequency::TWELVE_HOURS:
      return base::Hours(12);
    case PasswordConfirmationFrequency::TWO_DAYS:
      return base::Days(2);
    case PasswordConfirmationFrequency::WEEK:
      return base::Days(7);
  }
  NOTREACHED_IN_MIGRATION();
  return base::TimeDelta();
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  base::Value::List quick_unlock_modes_default;
  quick_unlock_modes_default.Append(kFactorsOptionAll);
  base::Value::List webauthn_factors_default;
  webauthn_factors_default.Append(kFactorsOptionAll);
  registry->RegisterListPref(prefs::kQuickUnlockModeAllowlist,
                             std::move(quick_unlock_modes_default));
  registry->RegisterListPref(prefs::kWebAuthnFactors,
                             std::move(webauthn_factors_default));
  registry->RegisterIntegerPref(
      prefs::kQuickUnlockTimeout,
      static_cast<int>(PasswordConfirmationFrequency::TWO_DAYS));

  // Preferences related the lock screen pin unlock.
  registry->RegisterIntegerPref(prefs::kPinUnlockMinimumLength,
                                kDefaultMinimumPinLength);
  // 0 indicates no maximum length for the pin.
  registry->RegisterIntegerPref(prefs::kPinUnlockMaximumLength, 0);
  registry->RegisterBooleanPref(prefs::kPinUnlockWeakPinsAllowed, true);

  registry->RegisterBooleanPref(::prefs::kPinUnlockAutosubmitEnabled, true);
}

bool IsPinDisabledByPolicy(PrefService* pref_service, Purpose purpose) {
  auto* test_api = TestApi::Get();
  if (test_api && test_api->IsQuickUnlockOverridden()) {
    return !test_api->IsPinEnabledByPolicy(purpose);
  }

  if (purpose == Purpose::kAny) {
    return IsPinDisabledByPolicySinglePurpose(pref_service, Purpose::kUnlock) &&
           IsPinDisabledByPolicySinglePurpose(pref_service, Purpose::kWebAuthn);
  }
  return IsPinDisabledByPolicySinglePurpose(pref_service, purpose);
}

bool IsPinEnabled() {
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
  if (location_info == "left-of-power-button-top-right")
    return FingerprintLocation::LEFT_OF_POWER_BUTTON_TOP_RIGHT;
  NOTREACHED_IN_MIGRATION() << "Not handled value: " << location_info;
  return default_location;
}

bool IsFingerprintSupported() {
  auto* test_api = TestApi::Get();
  if (test_api && test_api->IsQuickUnlockOverridden())
    return true;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return base::FeatureList::IsEnabled(features::kQuickUnlockFingerprint) &&
         command_line->HasSwitch(switches::kFingerprintSensorLocation);
}

bool IsFingerprintEnabled(Profile* profile, Purpose purpose) {
  // Don't need to check these when using flags to control fingerprint behavior
  // in tests.
  auto* test_api = TestApi::Get();
  if (!test_api || !test_api->IsQuickUnlockOverridden()) {
    if (!IsFingerprintSupported())
      return false;

    // Disable fingerprint if the profile does not belong to the primary user.
    if (profile != ProfileManager::GetPrimaryUserProfile())
      return false;
  }

  // Disable fingerprint if disallowed by policy.
  if (IsFingerprintDisabledByPolicy(profile->GetPrefs(), purpose))
    return false;

  return true;
}

void AddFingerprintResources(content::WebUIDataSource* html_source) {
  int resource_id;
  switch (GetFingerprintLocation()) {
    case FingerprintLocation::TABLET_POWER_BUTTON:
      resource_id = IDR_FINGERPRINT_TABLET_ANIMATION;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
      resource_id = IDR_FINGERPRINT_LAPTOP_BOTTOM_RIGHT_ANIMATION;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_LEFT:
      resource_id = IDR_FINGERPRINT_LAPTOP_BOTTOM_LEFT_ANIMATION;
      break;
    case FingerprintLocation::LEFT_OF_POWER_BUTTON_TOP_RIGHT:
      resource_id =
          IDR_FINGERPRINT_LAPTOP_LEFT_OF_POWER_BUTTON_TOP_RIGHT_ANIMATION;
      break;
    case FingerprintLocation::KEYBOARD_TOP_RIGHT:
    case FingerprintLocation::RIGHT_SIDE:
    case FingerprintLocation::LEFT_SIDE:
    case FingerprintLocation::UNKNOWN:
      resource_id = IDR_FINGERPRINT_DEFAULT_ANIMATION;
      break;
  }

  html_source->AddResourcePath("fingerprint_scanner_animation.json",
                               resource_id);

  // To use lottie, the worker-src CSP needs to be updated for the web ui
  // that is using it. Since as of now there are only a couple of webuis
  // using lottie animations, this update has to be performed manually. As
  // the usage increases, set this as the default so manual override is no
  // longer required.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");
}

FingerprintDescriptionStrings GetFingerprintDescriptionStrings(
    FingerprintLocation location) {
  auto location_string_it = kFingerprintLocationToStringsMap.find(location);
  CHECK(location_string_it != kFingerprintLocationToStringsMap.end());
  return location_string_it->second;
}

}  // namespace quick_unlock
}  // namespace ash

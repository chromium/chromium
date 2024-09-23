// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_

#include "base/memory/raw_ptr.h"

namespace base {
class TimeDelta;
}

namespace content {
class WebUIDataSource;
}  // namespace content

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace ash {
namespace quick_unlock {

// Enumeration specifying the purpose that the caller is using the credentials
// for.
enum class Purpose {
  kAny,
  kUnlock,
  kWebAuthn,
  // Total number of available purposes.
  kNumOfPurposes,
};

// Enumeration specifying the possible intervals before a strong auth
// (password) is required to use quick unlock. These values correspond to the
// policy items of QuickUnlockTimeout (policy ID 352) in policy_templates.json,
// and should be updated accordingly.
enum class PasswordConfirmationFrequency {
  SIX_HOURS = 0,
  TWELVE_HOURS = 1,
  TWO_DAYS = 2,
  WEEK = 3
};

// Enumeration specifying the possible fingerprint sensor locations.
enum class FingerprintLocation {
  TABLET_POWER_BUTTON = 0,
  KEYBOARD_BOTTOM_LEFT = 1,
  KEYBOARD_BOTTOM_RIGHT = 2,
  KEYBOARD_TOP_RIGHT = 3,
  RIGHT_SIDE = 4,
  LEFT_SIDE = 5,
  LEFT_OF_POWER_BUTTON_TOP_RIGHT = 6,
  UNKNOWN = 7,
};

// Struct that holds the description string IDs shown during the fingerprint
// setup.
struct FingerprintDescriptionStrings {
  int description_id = 0;
  int description_child_id = 0;
};

// Override quick unlock checks for testing.
class TestApi {
 public:
  // Setting state to true has an effect that instead of checking user prefs,
  // quick_unlock will use test flags to determine whether PIN/fingerprint is
  // enabled by policy. Setting to false resets the effect and quick_unlock will
  // resume to checking prefs, and also clears the previously set test flags.
  // Typical usage is setting state to true and enabling the purposes a test
  // needs by calling Enable*ByPolicyForTesting in SetUp, and setting state to
  // false in TearDown.
  explicit TestApi(bool override_quick_unlock);

  ~TestApi();

  static TestApi* Get();

  // Returns the current state of OverrideQuickUnlock.
  bool IsQuickUnlockOverridden();

  // Enable the specified purpose for PIN using a test flag. All purposes that
  // are not enabled will be treated as disabled when
  // EnablePinByPolicyUsingFlagsForTesting's state is true. When called, this
  // will automatically set OverrideQuickUnlock's state to true.
  void EnablePinByPolicy(Purpose purpose);

  // Enable the specified purpose for fingerprint using a test flag. All
  // purposes that are not enabled will be treated as disabled when
  // EnableFingerprintByPolicyUsingFlagsForTesting's state is true. When called,
  // this will automatically set OverrideQuickUnlock's state to true.
  void EnableFingerprintByPolicy(Purpose purpose);

  bool IsPinEnabledByPolicy(Purpose purpose);
  bool IsFingerprintEnabledByPolicy(Purpose purpose);

 private:
  static constexpr int kNumOfPurposes =
      static_cast<int>(Purpose::kNumOfPurposes);

  raw_ptr<TestApi> old_instance_;
  bool overridden_;
  bool pin_purposes_enabled_by_policy_[kNumOfPurposes];
  bool fingerprint_purposes_enabled_by_policy_[kNumOfPurposes];
};

base::TimeDelta PasswordConfirmationFrequencyToTimeDelta(
    PasswordConfirmationFrequency frequency);

// Register quick unlock prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if setting PIN is disabled by policy for the
// specified purpose. If purpose is kAny, then it is regarded as enabled if any
// of the purposes is enabled.
bool IsPinDisabledByPolicy(PrefService* pref_service, Purpose purpose);

// Returns true if the quick unlock feature flag is present.
// TODO(crbug.com/40709232): Remove this function because it always returns
// true.
bool IsPinEnabled();

// Returns true if the fingerprint is supported by the device.
bool IsFingerprintSupported();

// Returns true if the fingerprint is allowed for specified profile.
bool IsFingerprintEnabled(Profile* profile, Purpose purpose);

// Returns true if the fingerprint unlock is disabled by policy for the
// specified purpose. If purpose is kAny, then it is regarded as enabled if any
// of the purposes is enabled.
bool IsFingerprintDisabledByPolicy(const PrefService* pref_service,
                                   Purpose purpose);

// Returns fingerprint sensor location depending on the command line switch.
// Is used to display correct UI assets. Returns TABLET_POWER_BUTTON by default.
FingerprintLocation GetFingerprintLocation();

// Add fingerprint animations and illustrations. Used for the Fingerprint setup
// screen and the settings.
void AddFingerprintResources(content::WebUIDataSource* html_source);

// Returns the resource IDs for the fingerprint setup description strings.
FingerprintDescriptionStrings GetFingerprintDescriptionStrings(
    FingerprintLocation location);

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_

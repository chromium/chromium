// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_

namespace policy {

class AutoEnrollmentTypeChecker {
 public:
  // Parameter values for the kEnterpriseEnableUnifiedStateDetermination flag.
  static constexpr char kUnifiedStateDeterminationAlways[] = "always";
  static constexpr char kUnifiedStateDeterminationNever[] = "never";
  static constexpr char kUnifiedStateDeterminationOfficialBuild[] = "official";

  // Parameter values for the kEnterpriseEnableForcedReEnrollment flag.
  // Used for ChromeOS Flex.
  static constexpr char kFlexForcedReEnrollmentAlways[] = "always";
  static constexpr char kFlexForcedReEnrollmentNever[] = "never";

  // Status of the Unified State Determination.
  enum class USDStatus {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    kDisabledViaNeverSwitch = 0,
    // Deprecated: kDisabledViaKillSwitch = 1,
    kDisabledOnUnbrandedBuild = 2,
    kDisabledOnNonChromeDevice = 3,
    kEnabledOnOfficialGoogleChrome = 4,
    kEnabledOnOfficialGoogleFlex = 5,
    kEnabledViaAlwaysSwitch = 6,
    kMaxValue = kEnabledViaAlwaysSwitch
  };

  // Returns true if FRE state keys are supported.
  static bool AreFREStateKeysSupported();

  // Returns true when unified state determination is enabled based on
  // command-line switch, official build status and server-based kill-switch.
  static bool IsUnifiedStateDeterminationEnabled();

  // Returns true if any of Unified State Determination is enabled.
  static bool IsEnabled();
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_

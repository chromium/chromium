// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_

#include "base/functional/callback_forward.h"

namespace ash::system {
class StatisticsProvider;
}

namespace policy {

class AutoEnrollmentTypeChecker {
 public:
  // Parameter values for the kEnterpriseEnableUnifiedStateDetermination flag.
  static constexpr char kUnifiedStateDeterminationAlways[] = "always";
  static constexpr char kUnifiedStateDeterminationNever[] = "never";
  static constexpr char kUnifiedStateDeterminationOfficialBuild[] = "official";

  // Parameter values for the kEnterpriseEnableForcedReEnrollment flag.
  static constexpr char kForcedReEnrollmentAlways[] = "always";
  static constexpr char kForcedReEnrollmentNever[] = "never";
  static constexpr char kForcedReEnrollmentOfficialBuild[] = "official";

  // Parameter values for the kEnterpriseEnableInitialEnrollment flag.
  static constexpr char kInitialEnrollmentAlways[] = "always";
  static constexpr char kInitialEnrollmentNever[] = "never";
  static constexpr char kInitialEnrollmentOfficialBuild[] = "official";

  // Requirement for forced re-enrollment check.
  enum class FRERequirement {
    // FRE check is disabled by the OS command line.
    kDisabled = 0,
    // The device was setup (has kActivateDateKey) but doesn't have the
    // kCheckEnrollmentKey entry in VPD.
    kRequired = 1,
    // The device doesn't have kActivateDateKey, nor kCheckEnrollmentKey entry.
    kNotRequired = 2,
    // FRE check explicitly required by the flag in VPD or due to invalid VPD
    // state.
    kExplicitlyRequired = 3,
    // FRE check to be skipped, explicitly stated by the flag in VPD.
    kExplicitlyNotRequired = 4
  };

  // Type of auto enrollment or state determination check.
  enum class CheckType {
    kNone = 0,
    // Forced Re-Enrollment check implicitly required because the device is new.
    kForcedReEnrollmentImplicitlyRequired = 1,
    // Forced Re-Enrollment check explicitly required because the device was
    // previously enterprise-enrolled.
    kForcedReEnrollmentExplicitlyRequired = 2,
    // Initial state determination.
    kInitialStateDetermination = 3,
    // It is not known whether initial state determination is required because
    // the system clock is not synchronized.
    kUnknownDueToMissingSystemClockSync = 4,
  };

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

  // Returns true when unified state determination is enabled based on
  // command-line switch, official build status and server-based kill-switch.
  static bool IsUnifiedStateDeterminationEnabled();

  // Returns true if forced re-enrollment is enabled based on command-line
  // switch and official build status.
  static bool IsFREEnabled();

  // Returns true if initial enrollment is enabled based on command-line
  // switch and official build status.
  static bool IsInitialEnrollmentEnabled();

  // Returns true if any either FRE or initial enrollment are enabled.
  static bool IsEnabled();

  // Returns whether the FRE auto-enrollment check is required. Ignores all
  // command-line switches and checks the VPD directly. When kCheckEnrollmentKey
  // VPD entry is present, it is explicitly stating whether the forced
  // re-enrollment is required or not. Otherwise, for backward compatibility
  // with devices upgrading from an older version of ChromeOS, the
  // kActivateDateKey VPD entry is queried. If it's missing, FRE is not
  // required. This enables factories to start full guest sessions for testing,
  // see http://crbug.com/397354 for more context. The requirement for the
  // machine serial number to be present is a sanity-check to ensure that the
  // VPD has actually been read successfully. If VPD read failed, the FRE check
  // is required.
  //
  // Returns kExplicitlyRequired when unified state determination is enabled.
  // This allows legacy code to handle the unified state determination
  // correctly.
  static FRERequirement GetFRERequirementAccordingToVPD(
      ash::system::StatisticsProvider* statistics_provider);

  // Determines the type of auto-enrollment check that should be done. FRE has a
  // precedence over Initial state determination.
  // Returning CheckType::kUnknownDueToMissingSystemClockSync indicates that it
  // is not known yet whether Initial Enrollment should be done because the
  // system clock has not been synchronized yet. In this case, the caller is
  // supposed to call this again after the system clock has been synchronized.
  //
  // `dev_disable_boot == true` forces FRE unless explicitly disabled via
  // commandline switch.
  //
  // This method has a DCHECK that ensures that it is only called when unified
  // state determination is enabled.
  static CheckType DetermineAutoEnrollmentCheckType(
      bool is_system_clock_synchronized,
      ash::system::StatisticsProvider* statistics_provider,
      bool dev_disable_boot);

 private:
  // Requirement for initial state determination.
  enum class InitialStateDeterminationRequirement {
    // Initial state determination is disabled via command line.
    kDisabled = 0,
    // Initial state determination is not required.
    kNotRequired = 1,
    // Initial state determination is required.
    kRequired = 2,
    // It is not known whether initial state determination is required because
    // the system clock is not synchronized.
    kUnknownDueToMissingSystemClockSync = 3,
  };

  // Returns requirement for FRE.
  static FRERequirement GetFRERequirement(
      ash::system::StatisticsProvider* statistics_provider,
      bool dev_disable_boot);

  // Returns requirement for FRE on Flex. Note that this method doesn't check
  // whether it is running on Flex or not.
  static FRERequirement GetFRERequirementOnFlex();

  // Returns requirement for initial state determination.
  static InitialStateDeterminationRequirement
  GetInitialStateDeterminationRequirement(
      bool is_system_clock_synchronized,
      ash::system::StatisticsProvider* statistics_provider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_

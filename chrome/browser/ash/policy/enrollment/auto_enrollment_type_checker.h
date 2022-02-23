// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_

#include "base/command_line.h"

namespace policy {

class AutoEnrollmentTypeChecker {
 public:
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
    // The device was setup (has kActivateDateKey) but doesn't have the
    // kCheckEnrollmentKey entry in VPD, or the VPD is corrupted.
    kRequired,
    // The device doesn't have kActivateDateKey, nor kCheckEnrollmentKey entry
    // while the serial number has been successfully read from VPD.
    kNotRequired,
    // FRE check explicitly required by the flag in VPD.
    kExplicitlyRequired,
    // FRE check to be skipped, explicitly stated by the flag in VPD.
    kExplicitlyNotRequired
  };

  // Requirement for initial state determination.
  enum class InitialStateDeterminationRequirement {
    // Initial state determination is not required.
    kNotRequired,
    // Initial state determination is required.
    kRequired,
    // It is not known whether initial state determination would be required
    // because the system clock is not synchronized.
    kUnknownDueToMissingSystemClockSync,
  };

  // Type of auto enrollment or state determination check.
  enum class CheckType {
    kNone,
    // Forced Re-Enrollment check implicitly required because the device is new
    // or lost VPD state.
    kForcedReEnrollmentImplicitlyRequired,
    // Forced Re-Enrollment check explicitly required because the device was
    // previously enterprise-enrolled.
    kForcedReEnrollmentExplicitlyRequired,
    // Initial state determination.
    kInitialStateDetermination,
    // It is not known whether initial state determination would be required
    // because the system clock is not synchronized.
    kUnknownDueToMissingSystemClockSync,
  };

  // Returns true if forced re-enrollment is enabled based on command-line flags
  // and official build status.
  static bool IsFREEnabled();

  // Returns true if initial enrollment is enabled based on command-line
  // flags and official build status.
  static bool IsInitialEnrollmentEnabled();

  // Returns true if any either FRE or initial enrollment are enabled.
  static bool IsEnabled();

  // Returns whether the FRE auto-enrollment check is required. When
  // kCheckEnrollmentKey VPD entry is present, it is explicitly stating whether
  // the forced re-enrollment is required or not. Otherwise, for backward
  // compatibility with devices upgrading from an older version of Chrome OS,
  // the kActivateDateKey VPD entry is queried. If it's missing, FRE is not
  // required. This enables factories to start full guest sessions for testing,
  // see http://crbug.com/397354 for more context. The requirement for the
  // machine serial number to be present is a sanity-check to ensure that the
  // VPD has actually been read successfully. If VPD read failed, the FRE check
  // is required.
  static FRERequirement GetFRERequirementAccordingToVPD();

  // Determines the type of auto-enrollment check that should be done.
  // Returning AutoEnrollmentCheckType::kUnknownDueToMissingSystemClockSync
  // indicates that it is not known yet whether Initial Enrollment should be
  // done because the system clock has not been synchronized yet.
  // In this case, the caller is supposed to call this again after the system
  // clock has been synchronized.
  static CheckType DetermineAutoEnrollmentCheckType(
      bool is_system_clock_synchronized);

 private:
  // Returns whether the initial state determination is required.
  static InitialStateDeterminationRequirement
  GetInitialStateDeterminationRequirement(bool is_system_clock_synchronized);

  // Returns true if the FRE check should be done according to command-line
  // switches and device state.
  static bool ShouldDoFRECheck(base::CommandLine* command_line,
                               FRERequirement fre_requirement);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_TYPE_CHECKER_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/branding_buildflags.h"
#include "chromeos/system/factory_ping_embargo_check.h"
#include "chromeos/system/statistics_provider.h"

namespace ash::system {
// TODO(https://crbug.com/1164001): remove when migrated to ash::
using ::chromeos::system::FactoryPingEmbargoState;
using ::chromeos::system::GetEnterpriseManagementPingEmbargoState;
using ::chromeos::system::kFirmwareTypeKey;
using ::chromeos::system::kFirmwareTypeValueNonchrome;
using ::chromeos::system::kRlzBrandCodeKey;
}  // namespace ash::system

namespace {

// Returns true if this is an official build and the device has Chrome firmware.
bool IsGoogleBrandedChrome() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  std::string firmware_type;
  bool is_chrome_branded =
      !ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kFirmwareTypeKey, &firmware_type) ||
      firmware_type != ash::system::kFirmwareTypeValueNonchrome;
  return is_chrome_branded;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::string FRERequirementToString(
    policy::AutoEnrollmentTypeChecker::FRERequirement requirement) {
  using FRERequirement = policy::AutoEnrollmentTypeChecker::FRERequirement;
  switch (requirement) {
    case FRERequirement::kDisabled:
      return "Forced Re-Enrollment disabled via command line.";
    case FRERequirement::kRequired:
      return "Forced Re-Enrollment required.";
    case FRERequirement::kNotRequired:
      return "Forced Re-Enrollment disabled: first setup.";
    case FRERequirement::kExplicitlyRequired:
      return "Forced Re-Enrollment required: flag in VPD.";
    case FRERequirement::kExplicitlyNotRequired:
      return "Forced Re-Enrollment disabled: flag in VPD.";
  }
}

}  // namespace

namespace policy {

// static
bool AutoEnrollmentTypeChecker::IsFREEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment);
  if (command_line_mode == kForcedReEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kForcedReEnrollmentOfficialBuild) {
    return IsGoogleBrandedChrome();
  }

  if (command_line_mode == kForcedReEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Forced Re-Enrollment mode: " << command_line_mode
             << ".";
  return false;
}

// static
bool AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(
          ash::switches::kEnterpriseEnableInitialEnrollment))
    return IsGoogleBrandedChrome();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment);
  if (command_line_mode == kInitialEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kInitialEnrollmentOfficialBuild) {
    return IsGoogleBrandedChrome();
  }

  if (command_line_mode == kInitialEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Initial Enrollment mode: " << command_line_mode << ".";
  return false;
}

// static
bool AutoEnrollmentTypeChecker::IsEnabled() {
  return IsFREEnabled() || IsInitialEnrollmentEnabled();
}

// static
AutoEnrollmentTypeChecker::FRERequirement
AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD() {
  std::string check_enrollment_value;
  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  bool fre_flag_found = provider->GetMachineStatistic(
      ash::system::kCheckEnrollmentKey, &check_enrollment_value);

  if (fre_flag_found) {
    if (check_enrollment_value == "0")
      return FRERequirement::kExplicitlyNotRequired;
    if (check_enrollment_value == "1")
      return FRERequirement::kExplicitlyRequired;

    LOG(ERROR) << "Unexpected value for " << ash::system::kCheckEnrollmentKey
               << ": " << check_enrollment_value;
    LOG(WARNING) << "Forcing auto enrollment check.";
    return FRERequirement::kExplicitlyRequired;
  }
  // Assume that the presence of the machine serial number means that VPD has
  // been read successfully. Don't trust a missing ActivateDate if VPD could not
  // be read successfully.
  bool vpd_read_successfully = !provider->GetEnterpriseMachineID().empty();
  if (vpd_read_successfully &&
      !provider->GetMachineStatistic(ash::system::kActivateDateKey, nullptr)) {
    // The device has never been activated (enterprise enrolled or
    // consumer-owned) so doing a FRE check is not necessary.
    return FRERequirement::kNotRequired;
  }
  if (!vpd_read_successfully) {
    LOG(ERROR) << "VPD could not be read, skipping explicitly required auto "
                  "enrollment check.";
  }
  return FRERequirement::kRequired;
}

// static
AutoEnrollmentTypeChecker::FRERequirement
AutoEnrollmentTypeChecker::GetFRERequirement() {
  // Skip FRE check if it is not enabled by command-line switches.
  if (!IsFREEnabled()) {
    LOG(WARNING) << "FRE disabled.";
    return FRERequirement::kDisabled;
  }

  const auto fre_vpd_requirement =
      AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD();
  if (fre_vpd_requirement == FRERequirement::kExplicitlyNotRequired)
    return fre_vpd_requirement;

  // Skip FRE check if modulus configuration is not present.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentInitialModulus) &&
      !command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentModulusLimit)) {
    LOG(WARNING) << "FRE disabled through command line (config).";
    return FRERequirement::kNotRequired;
  }

  return fre_vpd_requirement;
}

// static
AutoEnrollmentTypeChecker::InitialStateDeterminationRequirement
AutoEnrollmentTypeChecker::GetInitialStateDeterminationRequirement(
    bool is_system_clock_synchronized) {
  // Skip Initial State Determination if it is not enabled according to
  // command-line flags.
  if (!IsInitialEnrollmentEnabled()) {
    LOG(WARNING) << "Initial Enrollment is disabled.";
    return InitialStateDeterminationRequirement::kDisabled;
  }

  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  const ash::system::FactoryPingEmbargoState embargo_state =
      ash::system::GetEnterpriseManagementPingEmbargoState(provider);
  if (provider->GetEnterpriseMachineID().empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing serial number.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  std::string rlz_brand_code;
  const bool rlz_brand_code_found = provider->GetMachineStatistic(
      ash::system::kRlzBrandCodeKey, &rlz_brand_code);
  if (!rlz_brand_code_found || rlz_brand_code.empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing brand code.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  switch (embargo_state) {
    case ash::system::FactoryPingEmbargoState::kMissingOrMalformed:
      LOG(WARNING) << "Initial State Determination required due to missing "
                      "embargo state.";
      return InitialStateDeterminationRequirement::kRequired;
    case ash::system::FactoryPingEmbargoState::kPassed:
      LOG(WARNING) << "Initial State Determination required due to passed "
                      "embargo state.";
      return InitialStateDeterminationRequirement::kRequired;
    case ash::system::FactoryPingEmbargoState::kNotPassed:
      if (!is_system_clock_synchronized) {
        LOG(WARNING) << "Cannot decide Initial State Determination due to out "
                        "of sync clock.";
        return InitialStateDeterminationRequirement::
            kUnknownDueToMissingSystemClockSync;
      }
      LOG(WARNING)
          << "Skip Initial State Determination due to invalid embargo date.";
      return InitialStateDeterminationRequirement::kNotRequired;
    case ash::system::FactoryPingEmbargoState::kInvalid:
      if (!is_system_clock_synchronized) {
        LOG(WARNING) << "Cannot decide Initial State Determination due to out "
                        "of sync clock.";
        return InitialStateDeterminationRequirement::
            kUnknownDueToMissingSystemClockSync;
      }
      LOG(WARNING) << "Skip Initial State Determination because the device is "
                      "in the embargo period.";
      return InitialStateDeterminationRequirement::kNotRequired;
  }
}

// static
AutoEnrollmentTypeChecker::CheckType
AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
    bool is_system_clock_synchronized) {
  // Skip everything if neither FRE nor Initial Enrollment are enabled.
  if (!IsEnabled()) {
    LOG(WARNING) << "Auto-enrollment disabled.";
    return CheckType::kNone;
  }

  // Skip everything if GAIA is disabled.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kDisableGaiaServices)) {
    LOG(WARNING) << "Auto-enrollment disabled: command line (gaia).";
    return CheckType::kNone;
  }

  // Determine whether to do an FRE check or an initial state determination.
  // FRE has precedence since managed devices must go through an FRE check.
  FRERequirement fre_requirement = GetFRERequirement();
  LOG(WARNING) << FRERequirementToString(fre_requirement);

  switch (fre_requirement) {
    case FRERequirement::kDisabled:
    case FRERequirement::kNotRequired:
      // Fall to the initial state determination check.
      break;
    case FRERequirement::kExplicitlyNotRequired:
      // Skip FRE check and initial determination check if the device is
      // explicitly marked as consumer owned.
      return CheckType::kNone;
    case FRERequirement::kExplicitlyRequired:
      LOG(WARNING) << "Proceeding with FRE check.";
      return CheckType::kForcedReEnrollmentExplicitlyRequired;
    case FRERequirement::kRequired:
      LOG(WARNING) << "Proceeding with FRE check.";
      return CheckType::kForcedReEnrollmentImplicitlyRequired;
  }

  // FRE is not required. Check whether an initial state determination should be
  // done.
  switch (
      GetInitialStateDeterminationRequirement(is_system_clock_synchronized)) {
    case InitialStateDeterminationRequirement::kDisabled:
    case InitialStateDeterminationRequirement::kNotRequired:
      return CheckType::kNone;
    case InitialStateDeterminationRequirement::
        kUnknownDueToMissingSystemClockSync:
      return CheckType::kUnknownDueToMissingSystemClockSync;
    case InitialStateDeterminationRequirement::kRequired:
      LOG(WARNING) << "Proceeding with Initial State Determination.";
      return CheckType::kInitialStateDetermination;
  }

  // Neither FRE nor initial state determination checks are needed.
  return CheckType::kNone;
}

}  // namespace policy

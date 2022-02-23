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
  std::string firmware_type;
  bool is_chrome_branded =
      !ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kFirmwareTypeKey, &firmware_type) ||
      firmware_type != ash::system::kFirmwareTypeValueNonchrome;
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_chrome_branded = false;
#endif
  return is_chrome_branded;
}

std::string FRERequirementToString(
    policy::AutoEnrollmentTypeChecker::FRERequirement requirement) {
  using FRERequirement = policy::AutoEnrollmentTypeChecker::FRERequirement;
  switch (requirement) {
    case FRERequirement::kRequired:
      return "Forced Re-Enrollment required.";
    case FRERequirement::kNotRequired:
      return "Forced Re-Enrollment disabled: first setup.";
    case FRERequirement::kExplicitlyRequired:
      return "Forced Re-Enrollment required: flag in VPD.";
    case FRERequirement::kExplicitlyNotRequired:
      return "Forced Re-Enrollment disabled: flag in VPD.";
  }

  NOTREACHED();
  return std::string();
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
AutoEnrollmentTypeChecker::InitialStateDeterminationRequirement
AutoEnrollmentTypeChecker::GetInitialStateDeterminationRequirement(
    bool is_system_clock_synchronized) {
  // Skip Initial State Determination if it is not enabled according to
  // command-line flags.
  if (!IsInitialEnrollmentEnabled()) {
    LOG(WARNING) << "Initial Enrollment is disabled.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  ash::system::FactoryPingEmbargoState embargo_state =
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

  if (!is_system_clock_synchronized &&
      (embargo_state == ash::system::FactoryPingEmbargoState::kInvalid ||
       embargo_state == ash::system::FactoryPingEmbargoState::kNotPassed)) {
    // Wait for the system clock to become synchronized and check again.
    LOG(WARNING)
        << "Skip Initial State Determination due to out of sync clock.";
    return InitialStateDeterminationRequirement::
        kUnknownDueToMissingSystemClockSync;
  }

  if (embargo_state == ash::system::FactoryPingEmbargoState::kInvalid) {
    LOG(WARNING)
        << "Skip Initial State Determination due to invalid embargo date.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }
  if (embargo_state == ash::system::FactoryPingEmbargoState::kNotPassed) {
    LOG(WARNING) << "Skip Initial State Determination because the device is in "
                    "the embargo period.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  LOG(WARNING) << "Initial State Determination required.";
  return InitialStateDeterminationRequirement::kRequired;
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
  FRERequirement fre_requirement = GetFRERequirementAccordingToVPD();
  LOG(WARNING) << FRERequirementToString(fre_requirement);

  // Skip FRE check if the device is explicitly marked as consumer owned.
  if (fre_requirement == FRERequirement::kExplicitlyNotRequired)
    return CheckType::kNone;

  if (ShouldDoFRECheck(command_line, fre_requirement)) {
    // FRE has precedence over Initial Enrollment.
    LOG(WARNING) << "Proceeding with FRE check.";
    return fre_requirement == FRERequirement::kExplicitlyRequired
               ? CheckType::kForcedReEnrollmentExplicitlyRequired
               : CheckType::kForcedReEnrollmentImplicitlyRequired;
  }

  // FRE is not required. Check whether an initial state determination should be
  // done.
  switch (
      GetInitialStateDeterminationRequirement(is_system_clock_synchronized)) {
    case InitialStateDeterminationRequirement::kRequired:
      LOG(WARNING) << "Proceeding with Initial State Determination.";
      return CheckType::kInitialStateDetermination;
    case InitialStateDeterminationRequirement::
        kUnknownDueToMissingSystemClockSync:
      return CheckType::kUnknownDueToMissingSystemClockSync;
    case InitialStateDeterminationRequirement::kNotRequired:
      break;
  }

  // Neither FRE nor initial state determination checks are needed.
  return CheckType::kNone;
}

// static
bool AutoEnrollmentTypeChecker::ShouldDoFRECheck(
    base::CommandLine* command_line,
    FRERequirement fre_requirement) {
  // Skip FRE check if modulus configuration is not present.
  if (!command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentInitialModulus) &&
      !command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentModulusLimit)) {
    LOG(WARNING) << "FRE disabled through command line (config).";
    return false;
  }

  // Skip FRE check if it is not enabled by command-line switches.
  if (!IsFREEnabled()) {
    LOG(WARNING) << "FRE disabled.";
    return false;
  }

  // Skip FRE check if explicitly not required to check.
  if (fre_requirement == FRERequirement::kExplicitlyNotRequired) {
    LOG(WARNING) << "FRE disabled for device in consumer mode.";
    return false;
  }

  // Skip FRE check if it is not required according to the device state.
  if (fre_requirement == FRERequirement::kNotRequired)
    return false;

  return true;
}

}  // namespace policy

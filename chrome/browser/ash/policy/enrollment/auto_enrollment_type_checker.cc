// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/system/factory_ping_embargo_check.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace policy {

namespace {

// Returns true if this is an official build and the device has Chrome firmware.
bool IsGoogleBrandedChrome() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  const absl::optional<base::StringPiece> firmware_type =
      ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kFirmwareTypeKey);
  return firmware_type != ash::system::kFirmwareTypeValueNonchrome;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::string FRERequirementToString(
    AutoEnrollmentTypeChecker::FRERequirement requirement) {
  using FRERequirement = AutoEnrollmentTypeChecker::FRERequirement;
  switch (requirement) {
    case FRERequirement::kDisabled:
      return "Forced Re-Enrollment disabled via command line.";
    case FRERequirement::kRequired:
      return "Forced Re-Enrollment required.";
    case FRERequirement::kNotRequired:
      return "Forced Re-Enrollment disabled: first setup.";
    case FRERequirement::kExplicitlyRequired:
      return "Forced Re-Enrollment explicitly required.";
    case FRERequirement::kExplicitlyNotRequired:
      return "Forced Re-Enrollment explicitly not required.";
  }
}

}  // namespace

// static
bool AutoEnrollmentTypeChecker::IsFREEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableForcedReEnrollment);
  if (command_line_mode == kForcedReEnrollmentAlways) {
    return true;
  }
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
AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
    ash::system::StatisticsProvider* statistics_provider) {
  const absl::optional<base::StringPiece> check_enrollment_value =
      statistics_provider->GetMachineStatistic(
          ash::system::kCheckEnrollmentKey);

  if (check_enrollment_value) {
    if (check_enrollment_value == "0") {
      return FRERequirement::kExplicitlyNotRequired;
    }
    if (check_enrollment_value == "1") {
      return FRERequirement::kExplicitlyRequired;
    }

    LOG(ERROR) << "Unexpected value for " << ash::system::kCheckEnrollmentKey
               << ": " << check_enrollment_value.value();
    LOG(WARNING) << "Forcing auto enrollment check.";
    return FRERequirement::kExplicitlyRequired;
  }

  // FRE fails on reven, do not force FRE check.
  if (ash::switches::IsRevenBranding() &&
      statistics_provider->GetVpdStatus() !=
          ash::system::StatisticsProvider::VpdStatus::kValid) {
    LOG(WARNING) << "Re-enrollment is not forced on reven device";
    return FRERequirement::kRequired;
  }

  // The FRE flag is not found. If VPD is in valid state, do not require FRE
  // check if the device was never owned. If VPD is broken, continue with FRE
  // check.
  switch (statistics_provider->GetVpdStatus()) {
    // If RO_VPD is broken, state keys are not available and FRE check
    // cannot start. To not to get stuck with forced re-enrollment, do not
    // enforce it and let users cancel in case of permanent error.
    case ash::system::StatisticsProvider::VpdStatus::kInvalid:
      // Both RO and RW VPDs are broken and state keys are not available.
      // Require re-enrollment but do not force it.
      LOG(WARNING) << "RO_VPD and RW_VPD are broken.";
      return FRERequirement::kRequired;
    case ash::system::StatisticsProvider::VpdStatus::kRoInvalid:
      // RO_VPD is broken, but RW_VPD is valid. `kActivateDateKey` indicating
      // ownership is available and trustworthy. Proceed with with ownership
      // check and require  re-enrollment if the device was owned.
      LOG(WARNING) << "RO_VPD is broken. Proceeding with ownership check.";
      [[fallthrough]];
    case ash::system::StatisticsProvider::VpdStatus::kValid:
      if (!statistics_provider->GetMachineStatistic(
              ash::system::kActivateDateKey)) {
        // The device has never been activated (enterprise enrolled or
        // consumer-owned) so doing a FRE check is not necessary.
        return FRERequirement::kNotRequired;
      }
      return FRERequirement::kRequired;
    case ash::system::StatisticsProvider::VpdStatus::kRwInvalid:
      // VPD is in invalid state and FRE flag cannot be assessed. Force FRE
      // check to prevent enrollment escapes.
      LOG(ERROR) << "VPD could not be read, forcing auto-enrollment check.";
      return FRERequirement::kExplicitlyRequired;
    case ash::system::StatisticsProvider::VpdStatus::kUnknown:
      NOTREACHED() << "VPD status is unknown";
      return FRERequirement::kRequired;
  }
}

// static
AutoEnrollmentTypeChecker::FRERequirement
AutoEnrollmentTypeChecker::GetFRERequirement(
    ash::system::StatisticsProvider* statistics_provider,
    bool dev_disable_boot) {
  // Skip FRE check if it is not enabled by command-line switches.
  if (!IsFREEnabled()) {
    LOG(WARNING) << "FRE disabled.";
    return FRERequirement::kDisabled;
  }

  // Skip FRE check if modulus configuration is not present.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentInitialModulus) &&
      !command_line->HasSwitch(
          ash::switches::kEnterpriseEnrollmentModulusLimit)) {
    LOG(WARNING) << "FRE disabled through command line (config).";
    return FRERequirement::kNotRequired;
  }

  // The FWMP flag DEVELOPER_DISABLE_BOOT indicates that FRE was configured
  // in the previous OOBE. We need to force FRE checks to prevent enrollment
  // escapes, see b/268267865.
  if (dev_disable_boot) {
    return FRERequirement::kExplicitlyRequired;
  }

  return AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
      statistics_provider);
}

// static
AutoEnrollmentTypeChecker::InitialStateDeterminationRequirement
AutoEnrollmentTypeChecker::GetInitialStateDeterminationRequirement(
    bool is_system_clock_synchronized,
    ash::system::StatisticsProvider* statistics_provider) {
  // Skip Initial State Determination if it is not enabled according to
  // command-line flags.
  if (!IsInitialEnrollmentEnabled()) {
    LOG(WARNING) << "Initial Enrollment is disabled.";
    return InitialStateDeterminationRequirement::kDisabled;
  }
  const ash::system::FactoryPingEmbargoState embargo_state =
      ash::system::GetEnterpriseManagementPingEmbargoState(statistics_provider);
  const absl::optional<base::StringPiece> serial_number =
      statistics_provider->GetMachineID();
  if (!serial_number || serial_number->empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing serial number.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  const absl::optional<base::StringPiece> rlz_brand_code =
      statistics_provider->GetMachineStatistic(ash::system::kRlzBrandCodeKey);
  if (!rlz_brand_code || rlz_brand_code->empty()) {
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
    bool is_system_clock_synchronized,
    ash::system::StatisticsProvider* statistics_provider,
    bool dev_disable_boot) {
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
  const FRERequirement fre_requirement =
      GetFRERequirement(statistics_provider, dev_disable_boot);
  LOG(WARNING) << FRERequirementToString(fre_requirement);

  switch (fre_requirement) {
    case FRERequirement::kDisabled:
    case FRERequirement::kNotRequired:
      // Fall to the initial state determination check.
      break;
    case FRERequirement::kExplicitlyNotRequired:
      // Force initial determination check even if explicitly not required.
      // TODO(igorcov): b/238592446 Return CheckType::kNone when that gets
      // fixed.
      break;
    case FRERequirement::kExplicitlyRequired:
      LOG(WARNING) << "Proceeding with FRE check.";
      return CheckType::kForcedReEnrollmentExplicitlyRequired;
    case FRERequirement::kRequired:
      LOG(WARNING) << "Proceeding with FRE check.";
      return CheckType::kForcedReEnrollmentImplicitlyRequired;
  }

  // FRE is not required. Check whether an initial state determination should be
  // done.
  switch (GetInitialStateDeterminationRequirement(is_system_clock_synchronized,
                                                  statistics_provider)) {
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

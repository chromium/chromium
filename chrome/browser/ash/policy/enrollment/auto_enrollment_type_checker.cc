// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/system/factory_ping_embargo_check.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Returns true if this is an official build and the device has Chrome firmware.
static bool IsOfficialGoogleChrome() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  const std::optional<std::string_view> firmware_type =
      ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kFirmwareTypeKey);
  return firmware_type != ash::system::kFirmwareTypeValueNonchrome;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// Returns true if this is an official Flex build.
static bool IsOfficialGoogleFlex() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  return ash::switches::IsRevenBranding();
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// Returns true if this is an official Google OS.
static bool IsOfficialGoogleOS() {
  return IsOfficialGoogleChrome() || IsOfficialGoogleFlex();
}

// A helpful function to dump FRE requirements in human readable form.
static std::string FRERequirementToString(
    AutoEnrollmentTypeChecker::FRERequirement requirement) {
  using FRERequirement = AutoEnrollmentTypeChecker::FRERequirement;
  switch (requirement) {
    case FRERequirement::kDisabled:
      return "Forced Re-Enrollment disabled by the OS or command line.";
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

// Returns true if we are on an officially branded Flex and FRE is enabled
// on Flex.
static bool IsOfficialGoogleFlexAndFREOnFlexIsEnabled() {
  return IsOfficialGoogleFlex() &&
         // FRE on Flex is enabled unless explicitly disabled ("never" enabled).
         base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             ash::switches::kEnterpriseEnableForcedReEnrollmentOnFlex) !=
             AutoEnrollmentTypeChecker::kForcedReEnrollmentNever;
}

// Returns true if FRE state keys are supported.
static bool AreFREStateKeysSupported() {
  // TODO(b/331677599): Return IsOfficialGoogleOS().
  return IsOfficialGoogleChrome() ||
         IsOfficialGoogleFlexAndFREOnFlexIsEnabled();
}

}  // namespace

// static
bool AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination);
  if (command_line_mode == kUnifiedStateDeterminationAlways) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kEnabledViaAlwaysSwitch);
    return true;
  }
  if (command_line_mode == kUnifiedStateDeterminationNever) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kDisabledViaNeverSwitch);
    return false;
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                USDStatus::kDisabledOnUnbrandedBuild);
#else
  if (IsOfficialGoogleChrome()) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kEnabledOnOfficialGoogleChrome);
  } else if (IsOfficialGoogleFlex()) {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kEnabledOnOfficialGoogleFlex);
  } else {
    base::UmaHistogramEnumeration(kUMAStateDeterminationStatus,
                                  USDStatus::kDisabledOnNonChromeDevice);
  }
#endif

  // Official Google OSes support unified state determination.
  return IsOfficialGoogleOS();
}

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
    return AreFREStateKeysSupported();
  }

  if (command_line_mode == kForcedReEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Forced Re-Enrollment mode: " << command_line_mode
             << ".";
}

// static
bool AutoEnrollmentTypeChecker::IsInitialEnrollmentEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      ash::switches::kEnterpriseEnableInitialEnrollment);
  if (command_line_mode == kInitialEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kInitialEnrollmentOfficialBuild) {
    return IsOfficialGoogleOS();
  }

  if (command_line_mode == kInitialEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Initial Enrollment mode: " << command_line_mode << ".";
}

// static
bool AutoEnrollmentTypeChecker::IsEnabled() {
  return IsFREEnabled() || IsInitialEnrollmentEnabled();
}

// static
AutoEnrollmentTypeChecker::FRERequirement
AutoEnrollmentTypeChecker::GetFRERequirementAccordingToVPD(
    ash::system::StatisticsProvider* statistics_provider) {
  // To support legacy code that does not support unified state determination
  // yet, we pretend FRE is explicitly required, when unified state
  // determination is enabled. For example, this disables powerwash and TPM
  // firmware updates during OOBE (since admin could have forbidden both).
  //
  // However, we don't do that if the platform doesn't support state keys,
  // because legacy state determination will not work then anyways.
  //
  // TODO(b/265923216): Migrate legacy code to support unified state
  // determination.
  if (IsUnifiedStateDeterminationEnabled()) {
    if (AreFREStateKeysSupported()) {
      LOG(WARNING) << "Unified state determination is enabled."
                      " Forcing legacy re-enrollment check.";
      return FRERequirement::kExplicitlyRequired;
    } else {
      LOG(WARNING) << "Unified state determination is enabled, but the device"
                      " does not support state keys."
                      " Disabling legacy re-enrollment check.";
      return FRERequirement::kDisabled;
    }
  }

  // FRE on Flex is not supported without unified state determination because
  // we do not have ways to store whether an FRE check should be done or not,
  // and the legacy path would then make a check on consumer devices without
  // using the PSM privacy-friendly protocol.
  if (ash::switches::IsRevenBranding()) {
    LOG(WARNING) << "Legacy re-enrollment on Flex is not supported.";
    return FRERequirement::kDisabled;
  }

  const std::optional<std::string_view> check_enrollment_value =
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
               << ": " << check_enrollment_value.value()
               << ". Forcing re-enrollment check.";
    return FRERequirement::kExplicitlyRequired;
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
      // TODO(crbug.com/40580068): It looks like this is hit on
      // ChromeSessionManagerRlzTest.DeviceIsUnlocked for instance (on the
      // "linux-chromeos-chrome" bot) but doesn't seem to be hit in the wild. If
      // the test setup is bad and this truly shouldn't be unreachable we should
      // upgrade this to a NOTREACHED(), otherwise we should probably add a
      // comment for why this can happen and remove the invariant.
      DUMP_WILL_BE_NOTREACHED() << "VPD status is unknown";
      return FRERequirement::kRequired;
  }
}

// static
AutoEnrollmentTypeChecker::FRERequirement
AutoEnrollmentTypeChecker::GetFRERequirement(
    ash::system::StatisticsProvider* statistics_provider,
    bool dev_disable_boot) {
  // Skip FRE check if FRE is not enabled on this device.
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
  // command-line switch.
  if (!IsInitialEnrollmentEnabled()) {
    LOG(WARNING) << "Initial Enrollment is disabled.";
    return InitialStateDeterminationRequirement::kDisabled;
  }
  const ash::system::FactoryPingEmbargoState embargo_state =
      ash::system::GetRlzPingEmbargoState(statistics_provider);
  const std::optional<std::string_view> serial_number =
      statistics_provider->GetMachineID();
  if (!serial_number || serial_number->empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing serial number.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  const std::optional<std::string_view> rlz_brand_code =
      statistics_provider->GetMachineStatistic(ash::system::kRlzBrandCodeKey);
  if (!rlz_brand_code || rlz_brand_code->empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing brand code.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  if (IsOfficialGoogleFlex()) {
    const std::string* enrollment_token =
        ash::OobeConfiguration::Get()->configuration().FindString(
            ash::configuration::kEnrollmentToken);
    if (!enrollment_token || enrollment_token->empty()) {
      LOG(WARNING) << "Skipping Initial State Determination on Flex as no Flex "
                      "token was found.";
      return InitialStateDeterminationRequirement::kNotRequired;
    }
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
      LOG(WARNING) << "Skip Initial State Determination because the device is "
                      "in the embargo period.";
      return InitialStateDeterminationRequirement::kNotRequired;
    case ash::system::FactoryPingEmbargoState::kInvalid:
      if (!is_system_clock_synchronized) {
        LOG(WARNING) << "Cannot decide Initial State Determination due to out "
                        "of sync clock.";
        return InitialStateDeterminationRequirement::
            kUnknownDueToMissingSystemClockSync;
      }
      LOG(WARNING) << "Skip Initial State Determination due to invalid embargo date.";
      return InitialStateDeterminationRequirement::kNotRequired;
  }
}

// static
AutoEnrollmentTypeChecker::CheckType
AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
    bool is_system_clock_synchronized,
    ash::system::StatisticsProvider* statistics_provider,
    bool dev_disable_boot) {
  // The only user of this function is AutoEnrollmentController and it should
  // not be calling it when unified state determination is enabled. Instead, we
  // fake explicitly forced re-enrollment to prevent users from skipping it.
  DCHECK(!IsUnifiedStateDeterminationEnabled());

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
      LOG(WARNING) << "Proceeding with explicit FRE check.";
      return CheckType::kForcedReEnrollmentExplicitlyRequired;
    case FRERequirement::kRequired:
      LOG(WARNING) << "Proceeding with implicit FRE check.";
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

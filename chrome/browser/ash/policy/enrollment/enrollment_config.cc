// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"

#include <optional>
#include <string>
#include <string_view>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_token_provider.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"

namespace policy {
namespace {

const char kRecoveryHistogram[] = "EnterpriseCheck.EnrollementRecoveryOnBoot";

// Do not reorder or delete entries because it is used in UMA.
enum class EnrollmentRecoveryOnBootUma {
  kForced = 0,
  kFalseFlag = 1,
  kNoSerialNumber = 2,
  kMaxValue = kNoSerialNumber,
};

std::string GetString(const base::Value::Dict& dict, std::string_view key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

bool IsEnrollingAfterRollback() {
  auto* login_display_host = ash::LoginDisplayHost::default_host();
  if (!login_display_host) {
    return false;
  }
  const auto* wizard_context = login_display_host->GetWizardContext();
  return wizard_context && ash::IsRollbackFlow(*wizard_context);
}

// Returns the license type to use based on the license type, assigned
// upgrade type and the license packaged from device state.
LicenseType GetLicenseTypeToUse(const std::string license_type,
                                const bool is_license_packaged_with_device,
                                const std::string assigned_upgrade_type) {
  if (license_type == kDeviceStateLicenseTypeEnterprise) {
    return LicenseType::kEnterprise;
  } else if (license_type == kDeviceStateLicenseTypeEducation) {
    return LicenseType::kEducation;
  } else if (license_type == kDeviceStateLicenseTypeTerminal) {
    return LicenseType::kTerminal;
  }

  if (!is_license_packaged_with_device &&
      assigned_upgrade_type == kDeviceStateAssignedUpgradeTypeKiosk) {
    return LicenseType::kTerminal;
  }

  return LicenseType::kNone;
}

std::string_view ToStringView(EnrollmentConfig::Mode mode) {
#define CASE(_name)                   \
  case EnrollmentConfig::Mode::_name: \
    return #_name;

  switch (mode) {
    CASE(MODE_NONE);
    CASE(MODE_MANUAL);
    CASE(MODE_MANUAL_REENROLLMENT);
    CASE(MODE_LOCAL_FORCED);
    CASE(MODE_LOCAL_ADVERTISED);
    CASE(MODE_SERVER_FORCED);
    CASE(MODE_SERVER_ADVERTISED);
    CASE(MODE_RECOVERY);
    CASE(MODE_ATTESTATION);
    CASE(MODE_ATTESTATION_LOCAL_FORCED);
    CASE(MODE_ATTESTATION_SERVER_FORCED);
    CASE(MODE_ATTESTATION_MANUAL_FALLBACK);
    CASE(MODE_INITIAL_SERVER_FORCED);
    CASE(MODE_ATTESTATION_INITIAL_SERVER_FORCED);
    CASE(MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK);
    CASE(MODE_ATTESTATION_ROLLBACK_FORCED);
    CASE(MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK);
    CASE(MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED);
    CASE(MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK);
  }

  NOTREACHED();
#undef CASE
}

EnrollmentConfig GetPrescribedRecoveryConfig(
    PrefService* local_state,
    const ash::InstallAttributes& install_attributes,
    ash::system::StatisticsProvider* statistics_provider) {
  EnrollmentConfig recovery_config;

  // Regardless what mode is applicable, the enrollment domain is fixed.
  recovery_config.management_domain = install_attributes.GetDomain();

  if (!local_state->GetBoolean(prefs::kEnrollmentRecoveryRequired)) {
    return recovery_config;
  }

  if (ash::DeviceSettingsService::IsInitialized() &&
      ash::DeviceSettingsService::Get()->HasDmToken()) {
    LOG(WARNING) << "False recovery flag.";
    local_state->ClearPref(::prefs::kEnrollmentRecoveryRequired);
    base::UmaHistogramEnumeration(kRecoveryHistogram,
                                  EnrollmentRecoveryOnBootUma::kFalseFlag);

    return recovery_config;
  }

  LOG(WARNING) << "Enrollment recovery required according to pref.";
  const auto serial_number = statistics_provider->GetMachineID();
  if (!serial_number || serial_number->empty()) {
    LOG(WARNING) << "Postponing recovery because machine id is missing.";
    base::UmaHistogramEnumeration(kRecoveryHistogram,
                                  EnrollmentRecoveryOnBootUma::kNoSerialNumber);
    return recovery_config;
  }

  recovery_config.mode = EnrollmentConfig::MODE_RECOVERY;
  base::UmaHistogramEnumeration(kRecoveryHistogram,
                                EnrollmentRecoveryOnBootUma::kForced);

  return recovery_config;
}

OOBEConfigSource ConvertToOOBEConfigSource(
    const std::string* oobe_config_source) {
  if (!oobe_config_source || oobe_config_source->empty()) {
    return OOBEConfigSource::kNone;
  }
  if (*oobe_config_source == "REMOTE_DEPLOYMENT") {
    return OOBEConfigSource::kRemoteDeployment;
  }
  if (*oobe_config_source == "PACKAGING_TOOL") {
    return OOBEConfigSource::kPackagingTool;
  }
  return OOBEConfigSource::kUnknown;
}

EnrollmentConfig::Mode GetManualFallbackMode(
    EnrollmentConfig::Mode attestation_mode) {
  switch (attestation_mode) {
    case EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED:
      return EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK;
    case EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED:
      return EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK;
    case EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED:
      return EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK;
    case EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED:
      return EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK;
    case EnrollmentConfig::MODE_NONE:
    case EnrollmentConfig::MODE_MANUAL:
    case EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
    case EnrollmentConfig::MODE_LOCAL_FORCED:
    case EnrollmentConfig::MODE_LOCAL_ADVERTISED:
    case EnrollmentConfig::MODE_SERVER_FORCED:
    case EnrollmentConfig::MODE_SERVER_ADVERTISED:
    case EnrollmentConfig::MODE_RECOVERY:
    case EnrollmentConfig::MODE_ATTESTATION:
    case EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED:
    case EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK:
    case EnrollmentConfig::MODE_INITIAL_SERVER_FORCED:
    case EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK:
    case EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK:
    case EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK:
      NOTREACHED_NORETURN()
          << "Mode does not have manual fallback: " << attestation_mode;
  }
}

}  // namespace

struct EnrollmentConfig::PrescribedConfig {
  EnrollmentConfig::Mode mode = MODE_NONE;
  std::string management_domain;
  std::string enrollment_token;
  OOBEConfigSource oobe_config_source = OOBEConfigSource::kNone;

  static PrescribedConfig GetPrescribedConfig(
      PrefService* local_state,
      ash::system::StatisticsProvider* statistics_provider,
      const base::Value::Dict& device_state,
      const ash::OobeConfiguration* oobe_configuration);
};

// static
EnrollmentConfig::PrescribedConfig
EnrollmentConfig::PrescribedConfig::GetPrescribedConfig(
    PrefService* local_state,
    ash::system::StatisticsProvider* statistics_provider,
    const base::Value::Dict& device_state,
    const ash::OobeConfiguration* oobe_configuration) {
  // Decide enrollment mode. Give precedence to forced variants.
  if (IsEnrollingAfterRollback()) {
    return {.mode = EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED};
  }

  const std::string device_state_mode =
      GetString(device_state, kDeviceStateMode);
  const std::string device_state_management_domain =
      GetString(device_state, kDeviceStateManagementDomain);

  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentEnforced) {
    return {.mode = EnrollmentConfig::MODE_SERVER_FORCED,
            .management_domain = device_state_management_domain};
  }

  if (device_state_mode == kDeviceStateInitialModeEnrollmentEnforced) {
    return {.mode = EnrollmentConfig::MODE_INITIAL_SERVER_FORCED,
            .management_domain = device_state_management_domain};
  }

  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentZeroTouch) {
    return {.mode = EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED,
            .management_domain = device_state_management_domain};
  }

  if (device_state_mode == kDeviceStateInitialModeEnrollmentZeroTouch) {
    return {.mode = EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED,
            .management_domain = device_state_management_domain};
  }

  if (device_state_mode == kDeviceStateInitialModeTokenEnrollment) {
    std::optional<std::string> enrollment_token =
        GetEnrollmentToken(oobe_configuration);
    // TODO(b/329271128): CHECK to ensure enrollment_token always has value
    // after this bug is fixed.
    if (enrollment_token.has_value()) {
      const std::string* oobe_config_source =
          oobe_configuration->configuration().FindString(
              ash::configuration::kSource);
      return {
          .mode = EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED,
          .enrollment_token = std::move(enrollment_token.value()),
          .oobe_config_source = ConvertToOOBEConfigSource(oobe_config_source)};
    } else {
      return {.mode = EnrollmentConfig::MODE_NONE};
    }
  }

  const bool pref_enrollment_auto_start_present =
      local_state->HasPrefPath(prefs::kDeviceEnrollmentAutoStart);
  const bool pref_enrollment_auto_start =
      local_state->GetBoolean(prefs::kDeviceEnrollmentAutoStart);

  const bool pref_enrollment_can_exit_present =
      local_state->HasPrefPath(prefs::kDeviceEnrollmentCanExit);
  const bool pref_enrollment_can_exit =
      local_state->GetBoolean(prefs::kDeviceEnrollmentCanExit);

  if (pref_enrollment_auto_start_present && pref_enrollment_auto_start &&
      pref_enrollment_can_exit_present && !pref_enrollment_can_exit) {
    return {.mode = EnrollmentConfig::MODE_LOCAL_FORCED};
  }

  const bool oem_is_managed = ash::system::StatisticsProvider::FlagValueToBool(
      statistics_provider->GetMachineFlag(
          ash::system::kOemIsEnterpriseManagedKey),
      /*default_value=*/false);
  const bool oem_can_exit_enrollment =
      ash::system::StatisticsProvider::FlagValueToBool(
          statistics_provider->GetMachineFlag(
              ash::system::kOemCanExitEnterpriseEnrollmentKey),
          /*default_value=*/true);

  if (oem_is_managed && !oem_can_exit_enrollment) {
    return {.mode = EnrollmentConfig::MODE_LOCAL_FORCED};
  }

  if (local_state->GetBoolean(ash::prefs::kOobeComplete)) {
    // If OOBE is complete, don't return advertised modes as there's currently
    // no way to make sure advertised enrollment only gets shown once.
    return {.mode = EnrollmentConfig::MODE_NONE};
  }

  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentRequested) {
    return {.mode = EnrollmentConfig::MODE_SERVER_ADVERTISED,
            .management_domain = device_state_management_domain};
  }

  if (pref_enrollment_auto_start_present && pref_enrollment_auto_start) {
    return {.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED};
  }

  if (oem_is_managed) {
    return {.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED};
  }

  return {.mode = EnrollmentConfig::MODE_NONE};
}

struct EnrollmentConfig::PrescribedLicense {
  bool is_license_packaged_with_device = false;
  EnrollmentConfig::AssignedUpgradeType assigned_upgrade_type =
      AssignedUpgradeType::kAssignedUpgradeTypeUnspecified;
  LicenseType license_type = LicenseType::kNone;

  static PrescribedLicense GetPrescribedLicense(
      const base::Value::Dict& device_state);
};

// static
EnrollmentConfig::PrescribedLicense
EnrollmentConfig::PrescribedLicense::GetPrescribedLicense(
    const base::Value::Dict& device_state) {
  EnrollmentConfig::AssignedUpgradeType assigned_upgrade_type =
      EnrollmentConfig::AssignedUpgradeType::
          kAssignedUpgradeTypeChromeEnterprise;

  const std::string assigned_upgrade_type_str =
      GetString(device_state, kDeviceStateAssignedUpgradeType);

  if (assigned_upgrade_type_str ==
      kDeviceStateAssignedUpgradeTypeChromeEnterprise) {
    assigned_upgrade_type = EnrollmentConfig::AssignedUpgradeType::
        kAssignedUpgradeTypeChromeEnterprise;
  } else if (assigned_upgrade_type_str ==
             kDeviceStateAssignedUpgradeTypeKiosk) {
    assigned_upgrade_type = EnrollmentConfig::AssignedUpgradeType::
        kAssignedUpgradeTypeKioskAndSignage;
  }

  const bool is_license_packaged_with_device =
      device_state.FindBool(kDeviceStatePackagedLicense).value_or(false);
  const std::string license_type_str =
      GetString(device_state, kDeviceStateLicenseType);

  const LicenseType license_type =
      GetLicenseTypeToUse(license_type_str, is_license_packaged_with_device,
                          assigned_upgrade_type_str);

  return {.is_license_packaged_with_device = is_license_packaged_with_device,
          .assigned_upgrade_type = assigned_upgrade_type,
          .license_type = license_type};
}

EnrollmentConfig::EnrollmentConfig() = default;
EnrollmentConfig::EnrollmentConfig(const EnrollmentConfig& other) = default;
EnrollmentConfig::~EnrollmentConfig() = default;

EnrollmentConfig::EnrollmentConfig(PrescribedConfig prescribed_config,
                                   PrescribedLicense prescribed_license)
    : mode(prescribed_config.mode),
      management_domain(std::move(prescribed_config.management_domain)),
      is_license_packaged_with_device(
          prescribed_license.is_license_packaged_with_device),
      license_type(prescribed_license.license_type),
      assigned_upgrade_type(prescribed_license.assigned_upgrade_type),
      enrollment_token(std::move(prescribed_config.enrollment_token)),
      oobe_config_source(prescribed_config.oobe_config_source) {}

// static
EnrollmentConfig EnrollmentConfig::GetPrescribedEnrollmentConfig() {
  return GetPrescribedEnrollmentConfig(
      g_browser_process->local_state(), *ash::InstallAttributes::Get(),
      ash::system::StatisticsProvider::GetInstance(),
      ash::OobeConfiguration::Get());
}

// static
EnrollmentConfig EnrollmentConfig::GetPrescribedEnrollmentConfig(
    PrefService* local_state,
    const ash::InstallAttributes& install_attributes,
    ash::system::StatisticsProvider* statistics_provider,
    const ash::OobeConfiguration* oobe_configuration) {
  DCHECK(local_state);
  DCHECK(statistics_provider);
  DCHECK(oobe_configuration);
  // If OOBE is done and the device is enrolled, check for need to recover
  // enrollment.
  if (local_state->GetBoolean(ash::prefs::kOobeComplete) &&
      install_attributes.IsCloudManaged()) {
    return GetPrescribedRecoveryConfig(local_state, install_attributes,
                                       statistics_provider);
  }

  const base::Value::Dict& device_state =
      local_state->GetDict(prefs::kServerBackedDeviceState);

  return EnrollmentConfig(
      PrescribedConfig::GetPrescribedConfig(local_state, statistics_provider,
                                            device_state, oobe_configuration),
      PrescribedLicense::GetPrescribedLicense(device_state));
}

// static
EnrollmentConfig EnrollmentConfig::GetDemoModeEnrollmentConfig() {
  EnrollmentConfig config;
  config.mode = EnrollmentConfig::MODE_ATTESTATION;
  config.management_domain = kDemoModeDomain;
  config.license_type = LicenseType::kEnterprise;
  return config;
}

EnrollmentConfig EnrollmentConfig::GetEffectiveConfig() const {
  if (should_enroll()) {
    return *this;
  }

  EnrollmentConfig manual_config = *this;
  // TODO(b/332529631): It is not possible to have `managed_domain` and
  // `MODE_NONE` at the same time. Figure out if `MODE_MANUAL_REENROLLMENT`
  // needs to be deprecated.
  manual_config.mode =
      management_domain.empty() ? MODE_MANUAL : MODE_MANUAL_REENROLLMENT;
  return manual_config;
}

EnrollmentConfig EnrollmentConfig::GetManualFallbackConfig() const {
  CHECK(is_mode_with_manual_fallback())
      << "Only automatic enrollment config can produce manual fallback config. "
         "Got "
      << *this;

  EnrollmentConfig manual_fallback_config = *this;
  manual_fallback_config.mode = GetManualFallbackMode(mode);

  return manual_fallback_config;
}

std::ostream& operator<<(std::ostream& os, const EnrollmentConfig::Mode& mode) {
  return os << ToStringView(mode);
}

std::ostream& operator<<(std::ostream& os, const EnrollmentConfig& config) {
  return os << "EnrollmentConfig(" << config.mode << ")";
}

}  // namespace policy

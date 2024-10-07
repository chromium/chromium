// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state_message_processor.h"

#include <memory>
#include <optional>
#include <string>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "components/policy/proto/device_management_backend.pb.h"

// TODO(crbug.com/40805389): Logging as "WARNING" throughout the file to make
// sure it's preserved in the logs.

namespace policy {

namespace {

namespace em = ::enterprise_management;

// Converts a restore mode enum value from the DM protocol for FRE into the
// corresponding prefs string constant.
std::string ConvertRestoreMode(
    em::DeviceStateRetrievalResponse::RestoreMode restore_mode) {
  switch (restore_mode) {
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE:
      return std::string();
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED:
      return kDeviceStateRestoreModeReEnrollmentRequested;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED:
      return kDeviceStateRestoreModeReEnrollmentEnforced;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED:
      return kDeviceStateModeDisabled;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
      return kDeviceStateRestoreModeReEnrollmentZeroTouch;
  }
}

// Converts a enterprise_management::LicenseType_LicenseTypeEnum
// for AutoEnrollment to it corresponding string.
std::string ConvertAutoEnrollmentLicenseType(
    ::enterprise_management::LicenseType_LicenseTypeEnum license_type) {
  switch (license_type) {
    case em::LicenseType::UNDEFINED:
      return std::string();
    case em::LicenseType::CDM_PERPETUAL:
      return kDeviceStateLicenseTypeEnterprise;
    case em::LicenseType::CDM_ANNUAL:
      return kDeviceStateLicenseTypeEnterprise;
    case em::LicenseType::KIOSK:
      return kDeviceStateLicenseTypeTerminal;
    case em::LicenseType::CDM_PACKAGED:
      return kDeviceStateLicenseTypeEnterprise;
  }
}

// Converts an initial enrollment mode enum value from the DM protocol for
// initial enrollment into the corresponding prefs string constant.
std::string ConvertInitialEnrollmentMode(
    em::DeviceInitialEnrollmentStateResponse::InitialEnrollmentMode
        initial_enrollment_mode) {
  switch (initial_enrollment_mode) {
    case em::DeviceInitialEnrollmentStateResponse::INITIAL_ENROLLMENT_MODE_NONE:
      return std::string();
    case em::DeviceInitialEnrollmentStateResponse::
        INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED:
      return kDeviceStateInitialModeEnrollmentEnforced;
    case em::DeviceInitialEnrollmentStateResponse::
        INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED:
      return kDeviceStateInitialModeEnrollmentZeroTouch;
    case em::DeviceInitialEnrollmentStateResponse::
        INITIAL_ENROLLMENT_MODE_DISABLED:
      return kDeviceStateModeDisabled;
    case em::DeviceInitialEnrollmentStateResponse::
        INITIAL_ENROLLMENT_MODE_TOKEN_ENROLLMENT_ENFORCED:
      return kDeviceStateInitialModeTokenEnrollment;
  }
}

// Converts an assigned upgrade type enum value from the DM protocol for
// initial enrollment into the corresponding prefs string constant.
std::string ConvertAssignedUpgradeType(
    em::DeviceInitialEnrollmentStateResponse::AssignedUpgradeType
        assigned_upgrade_type) {
  switch (assigned_upgrade_type) {
    case em::DeviceInitialEnrollmentStateResponse::
        ASSIGNED_UPGRADE_TYPE_UNSPECIFIED:
      return std::string();
    case em::DeviceInitialEnrollmentStateResponse::
        ASSIGNED_UPGRADE_TYPE_CHROME_ENTERPRISE:
      return kDeviceStateAssignedUpgradeTypeChromeEnterprise;
    case em::DeviceInitialEnrollmentStateResponse::
        ASSIGNED_UPGRADE_TYPE_KIOSK_AND_SIGNAGE:
      return kDeviceStateAssignedUpgradeTypeKiosk;
  }
}

// Converts a license packaging sku enum value from the DM protocol for initial
// enrollment into the corresponding prefs string constant.
std::string ConvertLicenseType(
    em::DeviceInitialEnrollmentStateResponse::LicensePackagingSKU license_sku) {
  switch (license_sku) {
    case em::DeviceInitialEnrollmentStateResponse::NOT_EXIST:
      return std::string();
    case em::DeviceInitialEnrollmentStateResponse::CHROME_ENTERPRISE:
      return kDeviceStateLicenseTypeEnterprise;
    case em::DeviceInitialEnrollmentStateResponse::CHROME_EDUCATION:
      return kDeviceStateLicenseTypeEducation;
    case em::DeviceInitialEnrollmentStateResponse::CHROME_TERMINAL:
      return kDeviceStateLicenseTypeTerminal;
  }
}

}  // namespace

// Generates a request to download the device state during Initial Enrollment.
class InitialEnrollmentStateMessageProcessor
    : public AutoEnrollmentStateMessageProcessor {
 public:
  InitialEnrollmentStateMessageProcessor(
      const std::string& device_serial_number,
      const std::string& device_brand_code,
      std::optional<std::string> flex_enrollment_token)
      : device_serial_number_(device_serial_number),
        device_brand_code_(device_brand_code),
        flex_enrollment_token_(std::move(flex_enrollment_token)) {}

  DeviceManagementService::JobConfiguration::JobType GetJobType()
      const override {
    return DeviceManagementService::JobConfiguration::
        TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void FillRequest(em::DeviceManagementRequest* request) override {
    auto* inner_request =
        request->mutable_device_initial_enrollment_state_request();
    inner_request->set_brand_code(device_brand_code_);
    inner_request->set_serial_number(device_serial_number_);
    if (flex_enrollment_token_.has_value()) {
      inner_request->set_enrollment_token(flex_enrollment_token_.value());
    }
  }

  std::optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) override {
    if (!response.has_device_initial_enrollment_state_response()) {
      LOG(ERROR) << "Server failed to provide initial enrollment response.";
      return std::nullopt;
    }

    return ParseInitialEnrollmentStateResponse(
        response.device_initial_enrollment_state_response());
  }

  static std::optional<ParsedResponse> ParseInitialEnrollmentStateResponse(
      const em::DeviceInitialEnrollmentStateResponse& state_response) {
    ParsedResponse parsed_response;

    if (state_response.has_initial_enrollment_mode()) {
      parsed_response.restore_mode = ConvertInitialEnrollmentMode(
          state_response.initial_enrollment_mode());
    } else {
      // Unknown initial enrollment mode - treat as no enrollment.
      parsed_response.restore_mode.clear();
    }

    if (state_response.has_management_domain())
      parsed_response.management_domain = state_response.management_domain();

    if (state_response.has_is_license_packaged_with_device()) {
      parsed_response.is_license_packaged_with_device =
          state_response.is_license_packaged_with_device();
    }

    if (state_response.has_license_packaging_sku()) {
      parsed_response.license_type =
          ConvertLicenseType(state_response.license_packaging_sku());
    }

    if (state_response.has_assigned_upgrade_type()) {
      parsed_response.assigned_upgrade_type =
          ConvertAssignedUpgradeType(state_response.assigned_upgrade_type());
    }

    if (state_response.has_disabled_state()) {
      parsed_response.disabled_message =
          state_response.disabled_state().message();
    }

    LOG(WARNING) << "Received initial_enrollment_mode="
                 << state_response.initial_enrollment_mode() << " ("
                 << parsed_response.restore_mode << "). ";

    LOG(WARNING) << (state_response.is_license_packaged_with_device()
                         ? "Device has a packaged license for management."
                         : "No packaged license. ");

    LOG(WARNING) << (state_response.has_assigned_upgrade_type()
                         ? base::StrCat(
                               {"Assigned upgrade type=",
                                base::NumberToString(
                                    state_response.assigned_upgrade_type()),
                                " (",
                                parsed_response.assigned_upgrade_type.value_or(
                                    std::string()),
                                ")."})
                         : "No assigned upgrade type.");

    return parsed_response;
  }

 private:
  // Serial number of the device.
  std::string device_serial_number_;
  // 4-character brand code of the device.
  std::string device_brand_code_;

  const std::optional<std::string> flex_enrollment_token_;
};

// Generates a request to download the device state during Forced Re-Enrollment
// (FRE).
class FREStateMessageProcessor : public AutoEnrollmentStateMessageProcessor {
 public:
  explicit FREStateMessageProcessor(const std::string& server_backed_state_key)
      : server_backed_state_key_(server_backed_state_key) {}

  DeviceManagementService::JobConfiguration::JobType GetJobType()
      const override {
    return DeviceManagementService::JobConfiguration::
        TYPE_DEVICE_STATE_RETRIEVAL;
  }

  void FillRequest(em::DeviceManagementRequest* request) override {
    request->mutable_device_state_retrieval_request()
        ->set_server_backed_state_key(server_backed_state_key_);
  }

  std::optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) override {
    if (!response.has_device_state_retrieval_response()) {
      LOG(ERROR) << "Server failed to provide auto-enrollment response.";
      return std::nullopt;
    }

    const em::DeviceStateRetrievalResponse& state_response =
        response.device_state_retrieval_response();
    const auto restore_mode = state_response.restore_mode();

    if (restore_mode == em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE &&
        state_response.has_initial_state_response()) {
      LOG(WARNING) << "Received restore_mode=" << restore_mode << " ("
                   << ConvertRestoreMode(restore_mode) << ")"
                   << " . Parsing included initial state response.";

      return InitialEnrollmentStateMessageProcessor::
          ParseInitialEnrollmentStateResponse(
              state_response.initial_state_response());
    } else {
      ParsedResponse parsed_response;

      parsed_response.restore_mode = ConvertRestoreMode(restore_mode);

      if (state_response.has_management_domain())
        parsed_response.management_domain = state_response.management_domain();

      if (state_response.has_disabled_state()) {
        parsed_response.disabled_message =
            state_response.disabled_state().message();
      }

      // Package license is not available during the re-enrollment
      parsed_response.is_license_packaged_with_device.reset();

      if (state_response.has_license_type()) {
        parsed_response.license_type = ConvertAutoEnrollmentLicenseType(
            state_response.license_type().license_type());
      }

      LOG(WARNING) << "Received restore_mode=" << restore_mode << " ("
                   << parsed_response.restore_mode << ").";

      return parsed_response;
    }
  }

 private:
  std::string server_backed_state_key_;
};

AutoEnrollmentStateMessageProcessor::ParsedResponse::ParsedResponse() = default;

AutoEnrollmentStateMessageProcessor::ParsedResponse::~ParsedResponse() =
    default;

AutoEnrollmentStateMessageProcessor::ParsedResponse::ParsedResponse(
    const ParsedResponse&) = default;

AutoEnrollmentStateMessageProcessor::~AutoEnrollmentStateMessageProcessor() =
    default;

// static
std::unique_ptr<AutoEnrollmentStateMessageProcessor>
AutoEnrollmentStateMessageProcessor::CreateForFRE(
    const std::string& server_backed_state_key) {
  return std::make_unique<FREStateMessageProcessor>(server_backed_state_key);
}

// static
std::unique_ptr<AutoEnrollmentStateMessageProcessor>
AutoEnrollmentStateMessageProcessor::CreateForInitialEnrollment(
    const std::string& device_serial_number,
    const std::string& device_brand_code,
    std::optional<std::string> flex_enrollment_token) {
  return std::make_unique<InitialEnrollmentStateMessageProcessor>(
      device_serial_number, device_brand_code,
      std::move(flex_enrollment_token));
}

}  // namespace policy

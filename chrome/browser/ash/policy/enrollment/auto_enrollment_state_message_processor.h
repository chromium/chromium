// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_MESSAGE_PROCESSOR_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_MESSAGE_PROCESSOR_H_

#include <memory>
#include <optional>
#include <string>

#include "components/policy/core/common/cloud/device_management_service.h"

namespace enterprise_management {
class DeviceManagementRequest;
class DeviceManagementResponse;
}  // namespace enterprise_management

namespace policy {

// Subclasses of this class generate the request to download the device state
// (after determining that there is server-side device state) and parse the
// response.
class AutoEnrollmentStateMessageProcessor {
 public:
  // Returns a message processor for Forced Re-enrollment.
  static std::unique_ptr<AutoEnrollmentStateMessageProcessor> CreateForFRE(
      const std::string& server_backed_state_key);

  // Returns a message processor for Initial enrollment.
  static std::unique_ptr<AutoEnrollmentStateMessageProcessor>
  CreateForInitialEnrollment(const std::string& device_serial_number,
                             const std::string& device_brand_code,
                             std::optional<std::string> flex_enrollment_token);

  virtual ~AutoEnrollmentStateMessageProcessor();

  // Parsed fields of DeviceManagementResponse.
  struct ParsedResponse {
    ParsedResponse();
    ParsedResponse(const ParsedResponse&);
    ~ParsedResponse();

    std::string restore_mode;
    std::optional<std::string> management_domain;
    std::optional<std::string> disabled_message;
    std::optional<bool> is_license_packaged_with_device;
    std::optional<std::string> license_type;
    std::optional<std::string> assigned_upgrade_type;
  };

  // Returns the request job type. This must match the request filled in
  // `FillRequest`.
  virtual DeviceManagementService::JobConfiguration::JobType GetJobType()
      const = 0;

  // Fills the specific request type in `request`.
  virtual void FillRequest(
      enterprise_management::DeviceManagementRequest* request) = 0;

  // Parses the `response`. If it is valid, returns an instance of
  // ParsedResponse. Otherwise, returns nullopt.
  virtual std::optional<ParsedResponse> ParseResponse(
      const enterprise_management::DeviceManagementResponse& response) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_STATE_MESSAGE_PROCESSOR_H_

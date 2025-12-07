// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_H_

#include <string>

#include "components/policy/core/common/cloud/device_management_service.h"

namespace policy {

// The real implementation of the device management service configuration that
// is used to create device management service instances.
class DeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  DeviceManagementServiceConfiguration(
      const std::string& dm_server_url,
      const std::string& realtime_reporting_server_url,
      const std::string& encrypted_reporting_server_url);
  DeviceManagementServiceConfiguration(
      const DeviceManagementServiceConfiguration&) = delete;
  DeviceManagementServiceConfiguration& operator=(
      const DeviceManagementServiceConfiguration&) = delete;
  ~DeviceManagementServiceConfiguration() override;

  std::string GetDMServerUrl() const override;
  std::string GetAgentParameter() const override;
  std::string GetPlatformParameter() const override;
  std::string GetRealtimeReportingServerUrl() const override;
  std::string GetEncryptedReportingServerUrl() const override;

 private:
  const std::string dm_server_url_;
  const std::string realtime_reporting_server_url_;
  const std::string encrypted_reporting_server_url_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_H_

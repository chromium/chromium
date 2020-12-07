// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace content {
class BrowserContext;
}

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
  ~DeviceManagementServiceConfiguration() override;

  std::string GetDMServerUrl() override;
  std::string GetAgentParameter() override;
  std::string GetPlatformParameter() override;
  std::string GetRealtimeReportingServerUrl() override;
  std::string GetEncryptedReportingServerUrl() override;
  std::string GetReportingConnectorServerUrl(
      content::BrowserContext* context) override;

 private:
  const std::string dm_server_url_;
  const std::string realtime_reporting_server_url_;
  const std::string encrypted_reporting_server_url_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementServiceConfiguration);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_CONFIGURATION_H_

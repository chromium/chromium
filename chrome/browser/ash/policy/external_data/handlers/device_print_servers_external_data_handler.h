// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINT_SERVERS_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINT_SERVERS_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_cloud_external_data_policy_handler.h"

namespace policy {

class PolicyService;

// This class observes the device setting "DeviceExternalPrintServers" and
// propagates data loaded from this external policy to appropriate objects.
class DevicePrintServersExternalDataHandler
    : public DeviceCloudExternalDataPolicyHandler {
 public:
  explicit DevicePrintServersExternalDataHandler(PolicyService* policy_service);
  DevicePrintServersExternalDataHandler(
      const DevicePrintServersExternalDataHandler&) = delete;
  DevicePrintServersExternalDataHandler& operator=(
      const DevicePrintServersExternalDataHandler&) = delete;
  ~DevicePrintServersExternalDataHandler() override;

  // DeviceCloudExternalDataPolicyHandler:
  void OnDeviceExternalDataSet(const std::string& policy) override;
  void OnDeviceExternalDataCleared(const std::string& policy) override;
  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override;
  void Shutdown() override;

 private:
  std::unique_ptr<DeviceCloudExternalDataPolicyObserver>
      device_print_servers_observer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINT_SERVERS_EXTERNAL_DATA_HANDLER_H_

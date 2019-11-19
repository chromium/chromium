// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_WILCO_DTC_CONFIGURATION_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_WILCO_DTC_CONFIGURATION_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/device_cloud_external_data_policy_handler.h"

namespace policy {

class PolicyService;

// This class observes the device setting "DeviceWilcoDtcConfiguration" and
// saves the wilco DTC (diagnostics and telemetry) controller configuration
// received in JSON format.
class DeviceWilcoDtcConfigurationExternalDataHandler final
    : public DeviceCloudExternalDataPolicyHandler {
 public:
  explicit DeviceWilcoDtcConfigurationExternalDataHandler(
      PolicyService* policy_service);
  ~DeviceWilcoDtcConfigurationExternalDataHandler() override;

  // DeviceCloudExternalDataPolicyHandler:
  void OnDeviceExternalDataCleared(const std::string& policy) override;
  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override;
  void Shutdown() override;

 private:
  std::unique_ptr<DeviceCloudExternalDataPolicyObserver>
      device_wilco_dtc_configuration_observer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceWilcoDtcConfigurationExternalDataHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_WILCO_DTC_CONFIGURATION_EXTERNAL_DATA_HANDLER_H_

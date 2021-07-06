// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINTERS_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINTERS_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_cloud_external_data_policy_handler.h"

namespace chromeos {
class BulkPrintersCalculator;
}  // namespace chromeos

namespace policy {

class PolicyService;

class DevicePrintersExternalDataHandler
    : public DeviceCloudExternalDataPolicyHandler {
 public:
  DevicePrintersExternalDataHandler(
      PolicyService* policy_service,
      base::WeakPtr<chromeos::BulkPrintersCalculator> device_calculator);
  ~DevicePrintersExternalDataHandler() override;

  // DeviceCloudExternalDataPolicyHandler:
  void OnDeviceExternalDataSet(const std::string& policy) override;
  void OnDeviceExternalDataCleared(const std::string& policy) override;
  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override;
  void Shutdown() override;

 private:
  base::WeakPtr<chromeos::BulkPrintersCalculator> calculator_;

  std::unique_ptr<DeviceCloudExternalDataPolicyObserver>
      device_printers_observer_;

  DISALLOW_COPY_AND_ASSIGN(DevicePrintersExternalDataHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINTERS_EXTERNAL_DATA_HANDLER_H_

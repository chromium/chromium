// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINTERS_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINTERS_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_cloud_external_data_policy_handler.h"

namespace ash {
class BulkPrintersCalculator;
}  // namespace ash

namespace policy {

class PolicyService;

class DevicePrintersExternalDataHandler
    : public DeviceCloudExternalDataPolicyHandler {
 public:
  DevicePrintersExternalDataHandler(
      PolicyService* policy_service,
      base::WeakPtr<ash::BulkPrintersCalculator> device_calculator);

  DevicePrintersExternalDataHandler(const DevicePrintersExternalDataHandler&) =
      delete;
  DevicePrintersExternalDataHandler& operator=(
      const DevicePrintersExternalDataHandler&) = delete;

  ~DevicePrintersExternalDataHandler() override;

  // DeviceCloudExternalDataPolicyHandler:
  void OnDeviceExternalDataSet(const std::string& policy) override;
  void OnDeviceExternalDataCleared(const std::string& policy) override;
  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override;
  void Shutdown() override;

 private:
  base::WeakPtr<ash::BulkPrintersCalculator> calculator_;

  std::unique_ptr<DeviceCloudExternalDataPolicyObserver>
      device_printers_observer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_DEVICE_PRINTERS_EXTERNAL_DATA_HANDLER_H_

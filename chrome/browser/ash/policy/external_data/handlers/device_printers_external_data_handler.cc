// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/device_printers_external_data_handler.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"
#include "components/policy/policy_constants.h"

namespace policy {

DevicePrintersExternalDataHandler::DevicePrintersExternalDataHandler(
    PolicyService* policy_service,
    base::WeakPtr<ash::BulkPrintersCalculator> calculator)
    : calculator_(calculator),
      device_printers_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDevicePrinters,
              this)) {}

DevicePrintersExternalDataHandler::~DevicePrintersExternalDataHandler() =
    default;

void DevicePrintersExternalDataHandler::OnDeviceExternalDataSet(
    const std::string& policy) {
  if (calculator_)
    calculator_->ClearData();
}

void DevicePrintersExternalDataHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  if (calculator_)
    calculator_->ClearData();
}

void DevicePrintersExternalDataHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  if (calculator_)
    calculator_->SetData(std::move(data));
}

void DevicePrintersExternalDataHandler::Shutdown() {
  device_printers_observer_.reset();
}

}  // namespace policy

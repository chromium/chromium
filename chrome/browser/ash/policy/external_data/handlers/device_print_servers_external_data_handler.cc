// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/device_print_servers_external_data_handler.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider_factory.h"
#include "components/policy/policy_constants.h"

namespace {

base::WeakPtr<ash::PrintServersProvider> GetDevicePrintServersProvider() {
  return ash::PrintServersProviderFactory::Get()->GetForDevice();
}

}  // namespace

namespace policy {

DevicePrintServersExternalDataHandler::DevicePrintServersExternalDataHandler(
    PolicyService* policy_service)
    : device_print_servers_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceExternalPrintServers,
              this)) {}

DevicePrintServersExternalDataHandler::
    ~DevicePrintServersExternalDataHandler() = default;

void DevicePrintServersExternalDataHandler::OnDeviceExternalDataSet(
    const std::string& policy) {
  GetDevicePrintServersProvider()->ClearData();
}

void DevicePrintServersExternalDataHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  GetDevicePrintServersProvider()->ClearData();
}

void DevicePrintServersExternalDataHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  GetDevicePrintServersProvider()->SetData(std::move(data));
}

void DevicePrintServersExternalDataHandler::Shutdown() {
  device_print_servers_observer_.reset();
}

}  // namespace policy

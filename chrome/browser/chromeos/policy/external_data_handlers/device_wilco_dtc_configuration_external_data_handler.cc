// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/external_data_handlers/device_wilco_dtc_configuration_external_data_handler.h"

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

chromeos::WilcoDtcSupportdManager* GetWilcoDtcSupportdManager() {
  chromeos::WilcoDtcSupportdManager* const wilco_dtc_supportd_manager =
      chromeos::WilcoDtcSupportdManager::Get();
  DCHECK(wilco_dtc_supportd_manager);
  return wilco_dtc_supportd_manager;
}

}  // namespace

DeviceWilcoDtcConfigurationExternalDataHandler::
    DeviceWilcoDtcConfigurationExternalDataHandler(
        PolicyService* policy_service)
    : device_wilco_dtc_configuration_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceWilcoDtcConfiguration,
              this)) {}

DeviceWilcoDtcConfigurationExternalDataHandler::
    ~DeviceWilcoDtcConfigurationExternalDataHandler() = default;

void DeviceWilcoDtcConfigurationExternalDataHandler::
    OnDeviceExternalDataCleared(const std::string& policy) {
  GetWilcoDtcSupportdManager()->SetConfigurationData(nullptr);
}

void DeviceWilcoDtcConfigurationExternalDataHandler::
    OnDeviceExternalDataFetched(const std::string& policy,
                                std::unique_ptr<std::string> data,
                                const base::FilePath& file_path) {
  GetWilcoDtcSupportdManager()->SetConfigurationData(std::move(data));
}

void DeviceWilcoDtcConfigurationExternalDataHandler::Shutdown() {
  device_wilco_dtc_configuration_observer_.reset();
}

}  // namespace policy

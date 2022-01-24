// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "chromeos/system/statistics_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Format MAC address from AA:AA:AA:AA:AA:AA into AAAAAAAAAAAA (12 digit string)
// The :'s should be removed from MAC addresses to match the format of
// reporting MAC addresses and corresponding VPD fields.
// TODO(crbug.com/1236180): Move FormatMacAddress deeper into the
// CloudPolicyClient.
void FormatMacAddress(std::string* mac_address) {
  base::ReplaceChars(*mac_address, ":", "", mac_address);
  DCHECK(mac_address->empty() || mac_address->size() == 12);
}

std::string GetMachineModel(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string machine_model;
  statistics_provider->GetMachineStatistic(chromeos::system::kHardwareClassKey,
                                           &machine_model);
  return machine_model;
}

std::string GetBrandCode(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string brand_code;
  statistics_provider->GetMachineStatistic(chromeos::system::kRlzBrandCodeKey,
                                           &brand_code);
  return brand_code;
}

std::string GetAttestedDeviceId(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string attested_device_id;
  statistics_provider->GetMachineStatistic(
      chromeos::system::kAttestedDeviceIdKey, &attested_device_id);
  return attested_device_id;
}

std::string GetEthernetMacAddress(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string ethernet_mac_address;
  statistics_provider->GetMachineStatistic(
      chromeos::system::kEthernetMacAddressKey, &ethernet_mac_address);
  FormatMacAddress(&ethernet_mac_address);
  return ethernet_mac_address;
}

std::string GetDockMacAddress(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string dock_mac_address;
  statistics_provider->GetMachineStatistic(chromeos::system::kDockMacAddressKey,
                                           &dock_mac_address);
  FormatMacAddress(&dock_mac_address);
  return dock_mac_address;
}

std::string GetManufactureDate(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string manufacture_date;
  statistics_provider->GetMachineStatistic(
      chromeos::system::kManufactureDateKey, &manufacture_date);
  return manufacture_date;
}

}  // namespace

namespace policy {

// static
std::unique_ptr<CloudPolicyClient> CreateDeviceCloudPolicyClientAsh(
    chromeos::system::StatisticsProvider* statistics_provider,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CloudPolicyClient::DeviceDMTokenCallback device_dm_token_callback) {
  return std::make_unique<CloudPolicyClient>(
      statistics_provider->GetEnterpriseMachineID(),
      GetMachineModel(statistics_provider), GetBrandCode(statistics_provider),
      GetAttestedDeviceId(statistics_provider),
      GetEthernetMacAddress(statistics_provider),
      GetDockMacAddress(statistics_provider),
      GetManufactureDate(statistics_provider), service, url_loader_factory,
      std::move(device_dm_token_callback));
}

}  // namespace policy

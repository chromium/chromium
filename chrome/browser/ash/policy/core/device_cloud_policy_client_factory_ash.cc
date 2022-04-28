// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "chromeos/system/statistics_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr int kDottedMacAddressSize = 17;

// Parses MAC address string frommated as AA:AA:AA:AA:AA:AA. Returns nullopt if
// `mac_address_string` is empty or ill-formated.
absl::optional<policy::CloudPolicyClient::MacAddress> ParseMacAddress(
    base::StringPiece mac_address_string) {
  if (mac_address_string.size() != kDottedMacAddressSize)
    return absl::nullopt;

  policy::CloudPolicyClient::MacAddress parsed_mac_address;
  base::span<policy::CloudPolicyClient::MacAddress::value_type>
      parsed_mac_address_span(parsed_mac_address);

  // Go through every 2 chars digit + 1 char separator. Check the separator is
  // correct. Parse the hex digit.
  for (int string_idx = 0, span_idx = 0;
       string_idx < mac_address_string.size() &&
       span_idx < parsed_mac_address.size();
       string_idx += 3, ++span_idx) {
    const int separator_idx = string_idx + 2;
    if (separator_idx < mac_address_string.size() &&
        mac_address_string[separator_idx] != ':') {
      return absl::nullopt;
    }

    if (!base::HexStringToSpan(mac_address_string.substr(string_idx, 2),
                               parsed_mac_address_span.subspan(span_idx, 1))) {
      return absl::nullopt;
    }
  }

  return parsed_mac_address;
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

absl::optional<policy::CloudPolicyClient::MacAddress> GetEthernetMacAddress(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string ethernet_mac_address;
  statistics_provider->GetMachineStatistic(
      chromeos::system::kEthernetMacAddressKey, &ethernet_mac_address);
  return ParseMacAddress(ethernet_mac_address);
}

absl::optional<policy::CloudPolicyClient::MacAddress> GetDockMacAddress(
    chromeos::system::StatisticsProvider* statistics_provider) {
  std::string dock_mac_address;
  statistics_provider->GetMachineStatistic(chromeos::system::kDockMacAddressKey,
                                           &dock_mac_address);
  return ParseMacAddress(dock_mac_address);
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

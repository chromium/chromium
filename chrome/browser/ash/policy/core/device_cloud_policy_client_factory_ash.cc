// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr int kDottedMacAddressSize = 17;

// Parses MAC address string frommated as AA:AA:AA:AA:AA:AA. Returns nullopt if
// `mac_address_string` is empty or ill-formated.
std::optional<policy::CloudPolicyClient::MacAddress> ParseMacAddress(
    std::string_view mac_address_string) {
  if (mac_address_string.size() != kDottedMacAddressSize)
    return std::nullopt;

  policy::CloudPolicyClient::MacAddress parsed_mac_address;
  base::span<policy::CloudPolicyClient::MacAddress::value_type>
      parsed_mac_address_span(parsed_mac_address);

  // Go through every 2 chars digit + 1 char separator. Check the separator is
  // correct. Parse the hex digit.
  for (size_t string_idx = 0, span_idx = 0;
       string_idx < mac_address_string.size() &&
       span_idx < parsed_mac_address.size();
       string_idx += 3, ++span_idx) {
    const size_t separator_idx = string_idx + 2;
    if (separator_idx < mac_address_string.size() &&
        mac_address_string[separator_idx] != ':') {
      return std::nullopt;
    }

    if (!base::HexStringToSpan(mac_address_string.substr(string_idx, 2),
                               parsed_mac_address_span.subspan(span_idx, 1))) {
      return std::nullopt;
    }
  }

  return parsed_mac_address;
}

std::string_view EmptyIfAbsent(std::optional<std::string_view> opt) {
  return opt.value_or(std::string_view());
}

}  // namespace

namespace policy {

// static
std::unique_ptr<CloudPolicyClient> CreateDeviceCloudPolicyClientAsh(
    ash::system::StatisticsProvider* statistics_provider,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CloudPolicyClient::DeviceDMTokenCallback device_dm_token_callback) {
  return std::make_unique<CloudPolicyClient>(
      EmptyIfAbsent(statistics_provider->GetMachineID()),
      EmptyIfAbsent(statistics_provider->GetMachineStatistic(
          ash::system::kHardwareClassKey)),
      EmptyIfAbsent(statistics_provider->GetMachineStatistic(
          ash::system::kRlzBrandCodeKey)),
      EmptyIfAbsent(statistics_provider->GetMachineStatistic(
          ash::system::kAttestedDeviceIdKey)),
      ParseMacAddress(EmptyIfAbsent(statistics_provider->GetMachineStatistic(
          ash::system::kEthernetMacAddressKey))),
      ParseMacAddress(EmptyIfAbsent(statistics_provider->GetMachineStatistic(
          ash::system::kDockMacAddressKey))),
      EmptyIfAbsent(statistics_provider->GetMachineStatistic(
          ash::system::kManufactureDateKey)),
      service, url_loader_factory, std::move(device_dm_token_callback));
}

}  // namespace policy

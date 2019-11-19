// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/hostname_handler.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/system/statistics_provider.h"

namespace {

constexpr char kAssetIDPlaceholder[] = "${ASSET_ID}";
constexpr char kMachineNamePlaceholder[] = "${MACHINE_NAME}";
constexpr char kSerialNumPlaceholder[] = "${SERIAL_NUM}";
constexpr char kMACAddressPlaceholder[] = "${MAC_ADDR}";
constexpr char kLocationPlaceholder[] = "${LOCATION}";

// As per RFC 1035, hostname should be 63 characters or less.
const int kMaxHostnameLength = 63;

bool inline IsValidHostnameCharacter(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '_' || c == '-';
}

bool IsValidHostname(const std::string& hostname) {
  if ((hostname.size() > kMaxHostnameLength) || (hostname.size() == 0))
    return false;
  if (hostname[0] == '-')
    return false;  // '-' is not valid for the first char
  for (const char& c : hostname) {
    if (!IsValidHostnameCharacter(c))
      return false;
  }
  return true;
}

}  // namespace

namespace policy {

HostnameHandler::HostnameHandler(chromeos::CrosSettings* cros_settings)
    : cros_settings_(cros_settings) {
  policy_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kDeviceHostnameTemplate,
      base::BindRepeating(&HostnameHandler::OnDeviceHostnamePropertyChanged,
                          weak_factory_.GetWeakPtr()));
  chromeos::NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);

  // Fire it once so we're sure we get an invocation on startup.
  OnDeviceHostnamePropertyChanged();
}

HostnameHandler::~HostnameHandler() {}

void HostnameHandler::Shutdown() {
  if (chromeos::NetworkHandler::IsInitialized()) {
    chromeos::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

// static
std::string HostnameHandler::FormatHostname(const std::string& name_template,
                                            const std::string& asset_id,
                                            const std::string& serial,
                                            const std::string& mac,
                                            const std::string& machine_name,
                                            const std::string& location) {
  std::string result = name_template;
  base::ReplaceSubstringsAfterOffset(&result, 0, kAssetIDPlaceholder, asset_id);
  base::ReplaceSubstringsAfterOffset(&result, 0, kSerialNumPlaceholder, serial);
  base::ReplaceSubstringsAfterOffset(&result, 0, kMACAddressPlaceholder, mac);
  base::ReplaceSubstringsAfterOffset(&result, 0, kMachineNamePlaceholder,
                                     machine_name);
  base::ReplaceSubstringsAfterOffset(&result, 0, kLocationPlaceholder,
                                     location);

  if (!IsValidHostname(result))
    return std::string();
  return result;
}

void HostnameHandler::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  OnDeviceHostnamePropertyChanged();
}

void HostnameHandler::OnDeviceHostnamePropertyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::BindRepeating(&HostnameHandler::OnDeviceHostnamePropertyChanged,
                              weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  // Continue when machine statistics are loaded, to avoid blocking.
  chromeos::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
          &HostnameHandler::
              OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded,
          weak_factory_.GetWeakPtr()));
}

void HostnameHandler::
    OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded() {
  std::string hostname_template;
  cros_settings_->GetString(chromeos::kDeviceHostnameTemplate,
                            &hostname_template);

  const std::string serial = chromeos::system::StatisticsProvider::GetInstance()
                                 ->GetEnterpriseMachineID();

  const std::string asset_id = g_browser_process->platform_part()
                                   ->browser_policy_connector_chromeos()
                                   ->GetDeviceAssetID();

  const std::string machine_name = g_browser_process->platform_part()
                                       ->browser_policy_connector_chromeos()
                                       ->GetMachineName();

  const std::string location = g_browser_process->platform_part()
                                   ->browser_policy_connector_chromeos()
                                   ->GetDeviceAnnotatedLocation();

  chromeos::NetworkStateHandler* handler =
      chromeos::NetworkHandler::Get()->network_state_handler();

  std::string mac = "MAC_unknown";
  const chromeos::NetworkState* network = handler->DefaultNetwork();
  if (network) {
    const chromeos::DeviceState* device =
        handler->GetDeviceState(network->device_path());
    if (device) {
      mac = device->mac_address();
      base::ReplaceSubstringsAfterOffset(&mac, 0, ":", "");
    }
  }

  handler->SetHostname(FormatHostname(hostname_template, asset_id, serial, mac,
                                      machine_name, location));
}

}  // namespace policy

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/wifi_network_configuration/fake_wifi_network_configuration_handler.h"

FakeWifiNetworkConfigurationHandler::FakeWifiNetworkConfigurationHandler()
    : last_wifi_credentials_attachment_(
          WifiCredentialsAttachment(kDefaultId,
                                    kDefaultSecurityType,
                                    kDefaultSsid)) {}

FakeWifiNetworkConfigurationHandler::~FakeWifiNetworkConfigurationHandler() =
    default;

void FakeWifiNetworkConfigurationHandler::ConfigureWifiNetwork(
    const WifiCredentialsAttachment& wifi_credentials_attachment,
    chromeos::network_config::mojom::CrosNetworkConfig::ConfigureNetworkCallback
        callback) {
  ++num_configure_network_calls_;
  last_wifi_credentials_attachment_ = wifi_credentials_attachment;
  std::move(callback).Run(guid_, error_message_);
}

void FakeWifiNetworkConfigurationHandler::SetOutput(
    const std::optional<std::string>& network_guid,
    const std::string& error_message) {
  guid_ = network_guid;
  error_message_ = error_message;
}

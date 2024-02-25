// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_WIFI_NETWORK_CONFIGURATION_FAKE_WIFI_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROME_BROWSER_NEARBY_SHARING_WIFI_NETWORK_CONFIGURATION_FAKE_WIFI_NETWORK_CONFIGURATION_HANDLER_H_

#include "chrome/browser/nearby_sharing/wifi_credentials_attachment.h"
#include "chrome/browser/nearby_sharing/wifi_network_configuration/wifi_network_configuration_handler.h"

namespace {

const uint64_t kDefaultId = 0;
const char kDefaultSsid[] = "not_set";
const WifiCredentialsAttachment::SecurityType kDefaultSecurityType =
    sharing::mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk;

}  // namespace

// Fake WifiNetworkConfigurationHandler for testing. The class takes in WiFi
// Credentials Attachment and the network guid and error message can be set.
// Using the ConfigureNetworkCallback, the outputs can be verified.
class FakeWifiNetworkConfigurationHandler
    : public WifiNetworkConfigurationHandler {
 public:
  FakeWifiNetworkConfigurationHandler();
  ~FakeWifiNetworkConfigurationHandler() override;

  // WifiNetworkConfigurationHandler:
  void ConfigureWifiNetwork(
      const WifiCredentialsAttachment& wifi_credentials_attachment,
      chromeos::network_config::mojom::CrosNetworkConfig::
          ConfigureNetworkCallback callback) override;

  void SetOutput(const std::optional<std::string>& network_guid,
                 const std::string& error_message);

  size_t num_configure_network_calls() const {
    return num_configure_network_calls_;
  }
  const WifiCredentialsAttachment last_attachment() const {
    return last_wifi_credentials_attachment_;
  }

 private:
  size_t num_configure_network_calls_ = 0;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_remote_;
  WifiCredentialsAttachment last_wifi_credentials_attachment_;
  std::optional<std::string> guid_ = "not set";
  std::string error_message_ = "not set";
};

#endif  //  CHROME_BROWSER_NEARBY_SHARING_WIFI_NETWORK_CONFIGURATION_FAKE_WIFI_NETWORK_CONFIGURATION_HANDLER_H_

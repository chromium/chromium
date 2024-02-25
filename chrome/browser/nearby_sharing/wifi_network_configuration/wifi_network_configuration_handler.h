// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_WIFI_NETWORK_CONFIGURATION_WIFI_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROME_BROWSER_NEARBY_SHARING_WIFI_NETWORK_CONFIGURATION_WIFI_NETWORK_CONFIGURATION_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/nearby_sharing/wifi_credentials_attachment.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// The WifiNetworkConfigurationHandler receives a WifiCredentialsAttachment
// and invokes the cros_network_config mojo interface to add the network
// configuration (SSID, SecurityType, password) to the Known Networks List.
class WifiNetworkConfigurationHandler {
 public:
  WifiNetworkConfigurationHandler();
  virtual ~WifiNetworkConfigurationHandler();
  WifiNetworkConfigurationHandler(const WifiNetworkConfigurationHandler&) =
      delete;

  // We guarantee that |callback| will not be invoked after the
  // WifiNetworkConfigurationHandler instance is destroyed.
  virtual void ConfigureWifiNetwork(
      const WifiCredentialsAttachment& wifi_credentials_attachment,
      chromeos::network_config::mojom::CrosNetworkConfig::
          ConfigureNetworkCallback callback);

 private:
  void OnConfigureWifiNetworkResult(
      chromeos::network_config::mojom::CrosNetworkConfig::
          ConfigureNetworkCallback callback,
      const std::optional<std::string>& network_guid,
      const std::string& error_message);

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_remote_;
  base::WeakPtrFactory<WifiNetworkConfigurationHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_WIFI_NETWORK_CONFIGURATION_WIFI_NETWORK_CONFIGURATION_HANDLER_H_

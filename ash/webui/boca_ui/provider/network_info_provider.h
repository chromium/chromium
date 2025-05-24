// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_PROVIDER_NETWORK_INFO_PROVIDER_H_
#define ASH_WEBUI_BOCA_UI_PROVIDER_NETWORK_INFO_PROVIDER_H_

#include <memory>

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::boca {

// This class is responsible for providing the network info to
// `BocaAppPageHandler`.
class NetworkInfoProvider
    : public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  using NetworkInfoCallback = base::RepeatingCallback<void(
      std::vector<ash::boca::mojom::NetworkInfoPtr>)>;

  explicit NetworkInfoProvider(NetworkInfoCallback network_info_callback);
  NetworkInfoProvider(const NetworkInfoProvider&) = delete;
  NetworkInfoProvider& operator=(const NetworkInfoProvider&) = delete;
  ~NetworkInfoProvider() override;

 private:
  // CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;

  void FetchActiveNetworkList();

  // Called when network info is received.
  const NetworkInfoCallback network_info_callback_;

  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;

  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};

  base::WeakPtrFactory<NetworkInfoProvider> weak_ptr_factory_{this};
};

}  // namespace ash::boca
#endif  // ASH_WEBUI_BOCA_UI_PROVIDER_NETWORK_INFO_PROVIDER_H_

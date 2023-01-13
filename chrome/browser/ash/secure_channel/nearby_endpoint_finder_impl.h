// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_ENDPOINT_FINDER_IMPL_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_ENDPOINT_FINDER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash {
namespace secure_channel {

// NearbyEndpointFinder implementation which uses a Mojo connection to the
// Nearby utility process to find the endpoint. This process consists of:
//   (1) Starting discovery, using an out-of-band Bluetooth mechanism.
//   (2) Injecting an endpoint using a Bluetooth address.
//   (3) Waiting for an endpoint to be found.
//   (4) Stopping discovery.
class NearbyEndpointFinderImpl
    : public NearbyEndpointFinder,
      public ::nearby::connections::mojom::EndpointDiscoveryListener {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyEndpointFinder> Create(
        const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
            nearby_connections);
    static void SetFactoryForTesting(Factory* test_factory);

    virtual ~Factory() = default;

   protected:
    virtual std::unique_ptr<NearbyEndpointFinder> CreateInstance(
        const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
            nearby_connections) = 0;
  };

  NearbyEndpointFinderImpl(
      const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
          nearby_connections);
  ~NearbyEndpointFinderImpl() override;

 private:
  friend class NearbyEndpointFinderImplTest;

  // NearbyEndpointFinder:
  void PerformFindEndpoint() override;

  // ::nearby::connections::mojom::EndpointDiscoveryListener:
  void OnEndpointFound(
      const std::string& endpoint_id,
      ::nearby::connections::mojom::DiscoveredEndpointInfoPtr info) override;
  void OnEndpointLost(const std::string& endpoint_id) override {}

  void OnStartDiscoveryResult(::nearby::connections::mojom::Status status);
  void OnInjectBluetoothEndpointResult(
      ::nearby::connections::mojom::Status status);
  void OnStopDiscoveryResult(
      ::nearby::connections::mojom::DiscoveredEndpointInfoPtr info,
      ::nearby::connections::mojom::Status status);

  mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>
      nearby_connections_;

  mojo::Receiver<::nearby::connections::mojom::EndpointDiscoveryListener>
      endpoint_discovery_listener_receiver_{this};
  bool is_discovery_active_ = false;
  std::string endpoint_id_;
  std::vector<uint8_t> endpoint_info_;

  base::WeakPtrFactory<NearbyEndpointFinderImpl> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_ENDPOINT_FINDER_IMPL_H_

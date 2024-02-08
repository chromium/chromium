// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_ENDPOINT_FINDER_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_ENDPOINT_FINDER_H_

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"

namespace ash {
namespace secure_channel {

// Discovers an endpoint corresponding to a remote device via the Nearby
// Connections library using the service ID corresponding to SecureChannel.
// Only one NearbyEndpointFinder is meant to be active at a given time. Note
// that this class does not implement timeouts for finding a connection.
class NearbyEndpointFinder {
 public:
  virtual ~NearbyEndpointFinder();

  // Callback which receives an endpoint ID and additional metadata about a
  // discovered endpoint.
  using EndpointCallback = base::OnceCallback<void(
      const std::string&,
      ::nearby::connections::mojom::DiscoveredEndpointInfoPtr)>;

  // Attempts to find an endpoint for the device with the provided Bluetooth
  // address, which is expected to be a 6-byte MAC address.
  void FindEndpoint(
      const std::vector<uint8_t>& remote_device_bluetooth_address,
      const std::vector<uint8_t>& eid,
      EndpointCallback success_callback,
      base::OnceCallback<void(::nearby::connections::mojom::Status)>
          failure_callback);

 protected:
  NearbyEndpointFinder();

  const std::vector<uint8_t>& remote_device_bluetooth_address() const {
    return remote_device_bluetooth_address_;
  }

  const std::vector<uint8_t>& eid() const { return eid_; }

  void NotifyEndpointFound(
      const std::string& endpoint_id,
      ::nearby::connections::mojom::DiscoveredEndpointInfoPtr info);
  void NotifyEndpointDiscoveryFailure(
      ::nearby::connections::mojom::Status status);

  virtual void PerformFindEndpoint() = 0;

 private:
  std::vector<uint8_t> remote_device_bluetooth_address_;
  std::vector<uint8_t> eid_;
  EndpointCallback success_callback_;
  base::OnceCallback<void(::nearby::connections::mojom::Status)>
      failure_callback_;
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_ENDPOINT_FINDER_H_

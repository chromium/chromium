// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder.h"

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

namespace ash {
namespace secure_channel {

NearbyEndpointFinder::NearbyEndpointFinder() = default;

NearbyEndpointFinder::~NearbyEndpointFinder() = default;

void NearbyEndpointFinder::FindEndpoint(
    const std::vector<uint8_t>& remote_device_bluetooth_address,
    const std::vector<uint8_t>& eid,
    EndpointCallback success_callback,
    base::OnceCallback<void(::nearby::connections::mojom::Status)>
        failure_callback) {
  // Only intended to be called once.
  DCHECK(remote_device_bluetooth_address_.empty());

  remote_device_bluetooth_address_ = remote_device_bluetooth_address;
  eid_ = eid;
  success_callback_ = std::move(success_callback);
  failure_callback_ = std::move(failure_callback);

  PerformFindEndpoint();
}

void NearbyEndpointFinder::NotifyEndpointFound(
    const std::string& endpoint_id,
    ::nearby::connections::mojom::DiscoveredEndpointInfoPtr info) {
  std::move(success_callback_).Run(endpoint_id, std::move(info));
}

void NearbyEndpointFinder::NotifyEndpointDiscoveryFailure(
    ::nearby::connections::mojom::Status status) {
  std::move(failure_callback_).Run(status);
}

}  // namespace secure_channel
}  // namespace ash

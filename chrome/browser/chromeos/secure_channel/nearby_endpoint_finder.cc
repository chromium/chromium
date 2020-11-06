// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/nearby_endpoint_finder.h"

namespace chromeos {
namespace secure_channel {

NearbyEndpointFinder::NearbyEndpointFinder() = default;

NearbyEndpointFinder::~NearbyEndpointFinder() = default;

void NearbyEndpointFinder::FindEndpoint(
    const std::vector<uint8_t>& remote_device_bluetooth_address,
    EndpointCallback success_callback,
    base::OnceClosure failure_callback) {
  // Only intended to be called once.
  DCHECK(remote_device_bluetooth_address_.empty());

  remote_device_bluetooth_address_ = remote_device_bluetooth_address;
  success_callback_ = std::move(success_callback);
  failure_callback_ = std::move(failure_callback);

  PerformFindEndpoint();
}

void NearbyEndpointFinder::NotifyEndpointFound(
    const std::string& endpoint_id,
    location::nearby::connections::mojom::DiscoveredEndpointInfoPtr info) {
  std::move(success_callback_).Run(endpoint_id, std::move(info));
}

void NearbyEndpointFinder::NotifyEndpointDiscoveryFailure() {
  std::move(failure_callback_).Run();
}

}  // namespace secure_channel
}  // namespace chromeos

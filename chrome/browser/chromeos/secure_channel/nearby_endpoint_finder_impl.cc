// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/nearby_endpoint_finder_impl.h"

#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/public/mojom/nearby_connector.mojom.h"

namespace chromeos {
namespace secure_channel {
namespace {

using location::nearby::connections::mojom::DiscoveredEndpointInfoPtr;
using location::nearby::connections::mojom::DiscoveryOptions;
using location::nearby::connections::mojom::MediumSelection;
using location::nearby::connections::mojom::Status;
using location::nearby::connections::mojom::Strategy;

NearbyEndpointFinderImpl::Factory* g_test_factory = nullptr;

void OnStopDiscoveryDestructorResult(Status status) {
  if (status != Status::kSuccess)
    PA_LOG(WARNING) << "Failed to stop discovery as part of destructor";
}

}  // namespace

// static
std::unique_ptr<NearbyEndpointFinder> NearbyEndpointFinderImpl::Factory::Create(
    const mojo::SharedRemote<
        location::nearby::connections::mojom::NearbyConnections>&
        nearby_connections) {
  if (g_test_factory)
    return g_test_factory->CreateInstance(nearby_connections);

  return base::WrapUnique(new NearbyEndpointFinderImpl(nearby_connections));
}

// static
void NearbyEndpointFinderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

NearbyEndpointFinderImpl::NearbyEndpointFinderImpl(
    const mojo::SharedRemote<
        location::nearby::connections::mojom::NearbyConnections>&
        nearby_connections)
    : nearby_connections_(nearby_connections) {}

NearbyEndpointFinderImpl::~NearbyEndpointFinderImpl() {
  if (is_discovery_active_) {
    nearby_connections_->StopDiscovery(
        mojom::kServiceId, base::BindOnce(&OnStopDiscoveryDestructorResult));
  }
}

void NearbyEndpointFinderImpl::PerformFindEndpoint() {
  is_discovery_active_ = true;
  nearby_connections_->StartDiscovery(
      mojom::kServiceId,
      DiscoveryOptions::New(Strategy::kP2pPointToPoint,
                            MediumSelection::New(/*bluetooth=*/true,
                                                 /*ble=*/false,
                                                 /*webrtc=*/false,
                                                 /*wifi_lan=*/false),
                            /*fast_advertisement_service_uuid=*/base::nullopt,
                            /*is_out_of_band_connection=*/true),
      endpoint_discovery_listener_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyEndpointFinderImpl::OnStartDiscoveryResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyEndpointFinderImpl::OnEndpointFound(const std::string& endpoint_id,
                                               DiscoveredEndpointInfoPtr info) {
  PA_LOG(VERBOSE) << "Found endpoint with ID " << endpoint_id
                  << ", stopping discovery";
  nearby_connections_->StopDiscovery(
      mojom::kServiceId,
      base::BindOnce(&NearbyEndpointFinderImpl::OnStopDiscoveryResult,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id,
                     std::move(info)));
}

void NearbyEndpointFinderImpl::OnStartDiscoveryResult(Status status) {
  if (status != Status::kSuccess) {
    PA_LOG(WARNING) << "Failed to start Nearby discovery: " << status;
    is_discovery_active_ = false;
    NotifyEndpointDiscoveryFailure();
    return;
  }

  PA_LOG(VERBOSE) << "Started Nearby discovery";

  nearby_connections_->InjectBluetoothEndpoint(
      mojom::kServiceId, remote_device_bluetooth_address(),
      base::BindOnce(&NearbyEndpointFinderImpl::OnInjectBluetoothEndpointResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyEndpointFinderImpl::OnInjectBluetoothEndpointResult(Status status) {
  if (status != Status::kSuccess) {
    PA_LOG(WARNING) << "Failed to inject Bluetooth endpoint: " << status;
    NotifyEndpointDiscoveryFailure();
    return;
  }

  PA_LOG(VERBOSE) << "Injected Bluetooth endpoint";
}

void NearbyEndpointFinderImpl::OnStopDiscoveryResult(
    const std::string& endpoint_id,
    location::nearby::connections::mojom::DiscoveredEndpointInfoPtr info,
    Status status) {
  is_discovery_active_ = false;

  if (status != Status::kSuccess) {
    PA_LOG(WARNING) << "Failed to stop Nearby discovery: " << status;
    NotifyEndpointDiscoveryFailure();
    return;
  }

  NotifyEndpointFound(endpoint_id, std::move(info));
}

}  // namespace secure_channel
}  // namespace chromeos

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder_impl.h"

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "chrome/browser/ash/secure_channel/util/histogram_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"

namespace ash {
namespace secure_channel {
namespace {

using ::nearby::connections::mojom::DiscoveredEndpointInfoPtr;
using ::nearby::connections::mojom::DiscoveryOptions;
using ::nearby::connections::mojom::MediumSelection;
using ::nearby::connections::mojom::Status;
using ::nearby::connections::mojom::Strategy;

NearbyEndpointFinderImpl::Factory* g_test_factory = nullptr;

const size_t kEndpointIdLength = 4u;
const size_t kEndpointInfoLength = 4u;

void OnStopDiscoveryDestructorResult(Status status) {
  util::RecordStopDiscoveryResult(status);

  if (status != Status::kSuccess)
    PA_LOG(WARNING) << "Failed to stop discovery as part of destructor";
}

std::string GenerateEndpointId() {
  // Generate a random array of bytes; as long as it is of size of at least
  // 3/4 kEndpointInfoLength, the final substring of the Base64-encoded array
  // will be of size kEndpointInfoLength.
  std::vector<uint8_t> raw_endpoint_info =
      base::RandBytesAsVector(kEndpointIdLength);

  // Return the first kEndpointIdLength characters of the Base64-encoded string.
  return base::Base64Encode(raw_endpoint_info).substr(0, kEndpointIdLength);
}

std::vector<uint8_t> GenerateEndpointInfo(const std::vector<uint8_t>& eid) {
  if (eid.size() < 2) {
    return base::RandBytesAsVector(kEndpointInfoLength);
  }

  std::vector<uint8_t> endpoint_info = {
      // version number
      1,
      // 2 bytes indicating the EID
      eid[0],
      eid[1],
  };

  return endpoint_info;
}

}  // namespace

// static
std::unique_ptr<NearbyEndpointFinder> NearbyEndpointFinderImpl::Factory::Create(
    const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
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
    const mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections>&
        nearby_connections)
    : nearby_connections_(nearby_connections),
      endpoint_id_(GenerateEndpointId()) {}

NearbyEndpointFinderImpl::~NearbyEndpointFinderImpl() {
  if (is_discovery_active_) {
    nearby_connections_->StopDiscovery(
        mojom::kServiceId, base::BindOnce(&OnStopDiscoveryDestructorResult));
  }
}

void NearbyEndpointFinderImpl::PerformFindEndpoint() {
  is_discovery_active_ = true;
  endpoint_info_ = GenerateEndpointInfo(eid());
  nearby_connections_->StartDiscovery(
      mojom::kServiceId,
      DiscoveryOptions::New(Strategy::kP2pPointToPoint,
                            MediumSelection::New(/*bluetooth=*/true,
                                                 /*ble=*/false,
                                                 /*webrtc=*/false,
                                                 /*wifi_lan=*/false,
                                                 /*wifi_direct=*/false),
                            /*fast_advertisement_service_uuid=*/std::nullopt,
                            /*is_out_of_band_connection=*/true),
      endpoint_discovery_listener_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyEndpointFinderImpl::OnStartDiscoveryResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyEndpointFinderImpl::OnEndpointFound(const std::string& endpoint_id,
                                               DiscoveredEndpointInfoPtr info) {
  // Only look for endpoints whose endpoint metadata field matches the
  // parameters passed to the InjectEndpoint() call.
  if (endpoint_id_ != endpoint_id || endpoint_info_ != info->endpoint_info)
    return;

  PA_LOG(VERBOSE) << "Found endpoint with ID " << endpoint_id_
                  << ", stopping discovery";
  nearby_connections_->StopDiscovery(
      mojom::kServiceId,
      base::BindOnce(&NearbyEndpointFinderImpl::OnStopDiscoveryResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(info)));
}

void NearbyEndpointFinderImpl::OnStartDiscoveryResult(Status status) {
  util::RecordStartDiscoveryResult(status);

  if (status != Status::kSuccess) {
    PA_LOG(WARNING) << "Failed to start Nearby discovery: " << status;
    is_discovery_active_ = false;
    NotifyEndpointDiscoveryFailure(status);
    return;
  }

  PA_LOG(VERBOSE) << "Started Nearby discovery";

  nearby_connections_->InjectBluetoothEndpoint(
      mojom::kServiceId, endpoint_id_, endpoint_info_,
      remote_device_bluetooth_address(),
      base::BindOnce(&NearbyEndpointFinderImpl::OnInjectBluetoothEndpointResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyEndpointFinderImpl::OnInjectBluetoothEndpointResult(Status status) {
  util::RecordInjectEndpointResult(status);

  if (status != Status::kSuccess) {
    PA_LOG(WARNING) << "Failed to inject Bluetooth endpoint: " << status;
    NotifyEndpointDiscoveryFailure(status);
    return;
  }

  PA_LOG(VERBOSE) << "Injected Bluetooth endpoint";
}

void NearbyEndpointFinderImpl::OnStopDiscoveryResult(
    ::nearby::connections::mojom::DiscoveredEndpointInfoPtr info,
    Status status) {
  util::RecordStopDiscoveryResult(status);

  is_discovery_active_ = false;

  if (status != Status::kSuccess) {
    PA_LOG(WARNING) << "Failed to stop Nearby discovery: " << status;
    NotifyEndpointDiscoveryFailure(status);
    return;
  }

  NotifyEndpointFound(endpoint_id_, std::move(info));
}

}  // namespace secure_channel
}  // namespace ash

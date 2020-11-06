// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/nearby_connection_broker_impl.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/secure_channel/nearby_endpoint_finder.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace secure_channel {
namespace {

NearbyConnectionBrokerImpl::Factory* g_test_factory = nullptr;

using location::nearby::connections::mojom::BytesPayload;
using location::nearby::connections::mojom::ConnectionInfoPtr;
using location::nearby::connections::mojom::ConnectionOptions;
using location::nearby::connections::mojom::DiscoveredEndpointInfoPtr;
using location::nearby::connections::mojom::Medium;
using location::nearby::connections::mojom::MediumSelection;
using location::nearby::connections::mojom::NearbyConnections;
using location::nearby::connections::mojom::Payload;
using location::nearby::connections::mojom::PayloadContent;
using location::nearby::connections::mojom::PayloadPtr;
using location::nearby::connections::mojom::PayloadTransferUpdatePtr;
using location::nearby::connections::mojom::Status;

}  // namespace

// static
std::unique_ptr<NearbyConnectionBroker>
NearbyConnectionBrokerImpl::Factory::Create(
    const std::vector<uint8_t>& bluetooth_public_address,
    NearbyEndpointFinder* endpoint_finder,
    mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
    const mojo::SharedRemote<NearbyConnections>& nearby_connections,
    base::OnceClosure on_connected_callback,
    base::OnceClosure on_disconnected_callback) {
  if (g_test_factory) {
    return g_test_factory->CreateInstance(
        bluetooth_public_address, endpoint_finder,
        std::move(message_sender_receiver), std::move(message_receiver_remote),
        nearby_connections, std::move(on_connected_callback),
        std::move(on_disconnected_callback));
  }

  return base::WrapUnique(new NearbyConnectionBrokerImpl(
      bluetooth_public_address, endpoint_finder,
      std::move(message_sender_receiver), std::move(message_receiver_remote),
      nearby_connections, std::move(on_connected_callback),
      std::move(on_disconnected_callback)));
}

// static
void NearbyConnectionBrokerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

NearbyConnectionBrokerImpl::NearbyConnectionBrokerImpl(
    const std::vector<uint8_t>& bluetooth_public_address,
    NearbyEndpointFinder* endpoint_finder,
    mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
    const mojo::SharedRemote<NearbyConnections>& nearby_connections,
    base::OnceClosure on_connected_callback,
    base::OnceClosure on_disconnected_callback)
    : NearbyConnectionBroker(bluetooth_public_address,
                             std::move(message_sender_receiver),
                             std::move(message_receiver_remote),
                             std::move(on_connected_callback),
                             std::move(on_disconnected_callback)),
      endpoint_finder_(endpoint_finder),
      nearby_connections_(nearby_connections) {
  TransitionToStatus(ConnectionStatus::kDiscoveringEndpoint);
  endpoint_finder_->FindEndpoint(
      bluetooth_public_address,
      base::BindOnce(&NearbyConnectionBrokerImpl::OnEndpointDiscovered,
                     base::Unretained(this)),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnDiscoveryFailure,
                     base::Unretained(this)));
}

NearbyConnectionBrokerImpl::~NearbyConnectionBrokerImpl() = default;

void NearbyConnectionBrokerImpl::TransitionToStatus(
    ConnectionStatus connection_status) {
  PA_LOG(VERBOSE) << "Nearby Connection status: " << connection_status_
                  << " => " << connection_status;
  connection_status_ = connection_status;
}

void NearbyConnectionBrokerImpl::TransitionToDisconnected() {
  TransitionToStatus(ConnectionStatus::kDisconnected);
  Disconnect();
}

void NearbyConnectionBrokerImpl::OnEndpointDiscovered(
    const std::string& endpoint_id,
    DiscoveredEndpointInfoPtr info) {
  DCHECK_EQ(ConnectionStatus::kDiscoveringEndpoint, connection_status_);

  remote_endpoint_id_ = endpoint_id;
  TransitionToStatus(ConnectionStatus::kRequestingConnection);

  nearby_connections_->RequestConnection(
      mojom::kServiceId, info->endpoint_info, remote_endpoint_id_,
      ConnectionOptions::New(MediumSelection::New(/*bluetooth=*/true,
                                                  /*ble=*/false,
                                                  /*webrtc=*/true,
                                                  /*wifi_lan=*/false),
                             bluetooth_public_address()),
      connection_lifecycle_listener_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnRequestConnectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnectionBrokerImpl::OnDiscoveryFailure() {
  DCHECK_EQ(ConnectionStatus::kDiscoveringEndpoint, connection_status_);
  TransitionToDisconnected();
}

void NearbyConnectionBrokerImpl::OnRequestConnectionResult(Status status) {
  if (status == Status::kSuccess) {
    DCHECK_EQ(ConnectionStatus::kRequestingConnection, connection_status_);
    TransitionToStatus(ConnectionStatus::kWaitingForConnectionInitiation);
    return;
  }

  PA_LOG(WARNING) << "RequestConnection() failed: " << status;
  TransitionToDisconnected();
}

void NearbyConnectionBrokerImpl::OnAcceptConnectionResult(Status status) {
  if (status == Status::kSuccess) {
    DCHECK_EQ(ConnectionStatus::kAcceptingConnection, connection_status_);
    TransitionToStatus(
        ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice);
    return;
  }

  PA_LOG(WARNING) << "AcceptConnection() failed: " << status;
  TransitionToDisconnected();
}

void NearbyConnectionBrokerImpl::OnSendPayloadResult(
    SendMessageCallback callback,
    Status status) {
  bool success = status == Status::kSuccess;
  std::move(callback).Run(success);

  if (success)
    return;

  PA_LOG(WARNING) << "OnSendPayloadResult() failed: " << status;
  TransitionToDisconnected();
}

void NearbyConnectionBrokerImpl::SendMessage(const std::string& message,
                                             SendMessageCallback callback) {
  DCHECK_EQ(ConnectionStatus::kConnected, connection_status_);

  std::vector<uint8_t> message_as_bytes(message.begin(), message.end());

  nearby_connections_->SendPayload(
      mojom::kServiceId, std::vector<std::string>{remote_endpoint_id_},
      Payload::New(
          next_sent_payload_id_++,
          PayloadContent::NewBytes(BytesPayload::New(message_as_bytes))),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnSendPayloadResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NearbyConnectionBrokerImpl::OnConnectionInitiated(
    const std::string& endpoint_id,
    ConnectionInfoPtr info) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnConnectionInitiated(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  DCHECK_EQ(ConnectionStatus::kWaitingForConnectionInitiation,
            connection_status_);
  TransitionToStatus(ConnectionStatus::kAcceptingConnection);

  nearby_connections_->AcceptConnection(
      mojom::kServiceId, remote_endpoint_id_,
      payload_listener_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnAcceptConnectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnectionBrokerImpl::OnConnectionAccepted(
    const std::string& endpoint_id) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnConnectionAccepted(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  DCHECK_EQ(ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice,
            connection_status_);
  TransitionToStatus(ConnectionStatus::kConnected);

  NotifyConnected();
}

void NearbyConnectionBrokerImpl::OnConnectionRejected(
    const std::string& endpoint_id,
    Status status) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnConnectionRejected(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  PA_LOG(WARNING) << "Connection rejected: " << status;
  TransitionToDisconnected();
}

void NearbyConnectionBrokerImpl::OnDisconnected(
    const std::string& endpoint_id) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnDisconnected(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  PA_LOG(WARNING) << "Connection disconnected";
  TransitionToDisconnected();
}

void NearbyConnectionBrokerImpl::OnBandwidthChanged(
    const std::string& endpoint_id,
    Medium medium) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnBandwidthChanged(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  PA_LOG(VERBOSE) << "Bandwidth changed: " << medium;
}

void NearbyConnectionBrokerImpl::OnPayloadReceived(
    const std::string& endpoint_id,
    PayloadPtr payload) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnPayloadReceived(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  // TODO(khorimoto): Handle received payloads.
}

void NearbyConnectionBrokerImpl::OnPayloadTransferUpdate(
    const std::string& endpoint_id,
    PayloadTransferUpdatePtr update) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnPayloadTransferUpdate(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  // TODO(khorimoto): Handle received payload updates.
}

std::ostream& operator<<(std::ostream& stream,
                         NearbyConnectionBrokerImpl::ConnectionStatus status) {
  switch (status) {
    case NearbyConnectionBrokerImpl::ConnectionStatus::kUninitialized:
      stream << "[Uninitialized]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::kDiscoveringEndpoint:
      stream << "[Discovering endpoint]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::kRequestingConnection:
      stream << "[Requesting connection]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::
        kWaitingForConnectionInitiation:
      stream << "[Waiting for connection initiation]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::kAcceptingConnection:
      stream << "[Accepting connection]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::
        kWaitingForConnectionToBeAcceptedByRemoteDevice:
      stream << "[Waiting for connection to be accepted]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::kConnected:
      stream << "[Connected]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::kDisconnected:
      stream << "[Disconnected]";
      break;
  }
  return stream;
}

}  // namespace secure_channel
}  // namespace chromeos

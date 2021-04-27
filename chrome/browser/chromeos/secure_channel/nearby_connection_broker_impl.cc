// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/nearby_connection_broker_impl.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chrome/browser/chromeos/secure_channel/nearby_endpoint_finder.h"
#include "chrome/browser/chromeos/secure_channel/util/histogram_util.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace secure_channel {
namespace {

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

NearbyConnectionBrokerImpl::Factory* g_test_factory = nullptr;

constexpr base::TimeDelta kConnectionStatusChangeTimeout =
    base::TimeDelta::FromSeconds(10);

// The amount of time by which we can expect a WebRTC upgrade to have been
// completed. According to metrics, 30 seconds is the 95th+ percentile of how
// long it takes to upgrade to WebRTC.
constexpr base::TimeDelta kWebRtcUpgradeDelay =
    base::TimeDelta::FromSeconds(30);

// Numerical values should not be reused or changed since this is used by
// metrics.
enum class ConnectionMedium {
  kConnectedViaBluetooth = 0,
  kUpgradedToWebRtc = 1,
  kDisconnectedInUnder30Seconds = 2,
  kMaxValue = kDisconnectedInUnder30Seconds
};

void RecordConnectionMediumMetric(ConnectionMedium medium) {
  base::UmaHistogramEnumeration(
      "MultiDevice.SecureChannel.Nearby.ConnectionMedium", medium);
}

void RecordWebRtcUpgradeDuration(base::TimeDelta duration) {
  // Note: min/max/bucket values should not be changed. If they need to be
  // adjusted, a new histogram should be created.
  base::UmaHistogramCustomTimes(
      "MultiDevice.SecureChannel.Nearby.WebRtcUpgradeDuration", duration,
      /*min=*/base::TimeDelta::FromSeconds(1),
      /*max=*/base::TimeDelta::FromMinutes(5),
      /*buckets=*/50);
}

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
    base::OnceClosure on_disconnected_callback,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (g_test_factory) {
    return g_test_factory->CreateInstance(
        bluetooth_public_address, endpoint_finder,
        std::move(message_sender_receiver), std::move(message_receiver_remote),
        nearby_connections, std::move(on_connected_callback),
        std::move(on_disconnected_callback), std::move(timer));
  }

  return base::WrapUnique(new NearbyConnectionBrokerImpl(
      bluetooth_public_address, endpoint_finder,
      std::move(message_sender_receiver), std::move(message_receiver_remote),
      nearby_connections, std::move(on_connected_callback),
      std::move(on_disconnected_callback), std::move(timer)));
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
    base::OnceClosure on_disconnected_callback,
    std::unique_ptr<base::OneShotTimer> timer)
    : NearbyConnectionBroker(bluetooth_public_address,
                             std::move(message_sender_receiver),
                             std::move(message_receiver_remote),
                             std::move(on_connected_callback),
                             std::move(on_disconnected_callback)),
      endpoint_finder_(endpoint_finder),
      nearby_connections_(nearby_connections),
      timer_(std::move(timer)) {
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
  PA_LOG(INFO) << "Nearby Connection status: " << connection_status_ << " => "
               << connection_status;
  connection_status_ = connection_status;

  timer_->Stop();

  // The connected and disconnected states do not expect any further state
  // changes.
  if (connection_status_ == ConnectionStatus::kConnected ||
      connection_status_ == ConnectionStatus::kDisconnected) {
    return;
  }

  // If the state does not change within |kConnectionStatusChangeTimeout|, time
  // out and give up on the connection.
  timer_->Start(
      FROM_HERE, kConnectionStatusChangeTimeout,
      base::BindOnce(
          &NearbyConnectionBrokerImpl::OnConnectionStatusChangeTimeout,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnectionBrokerImpl::Disconnect(
    util::NearbyDisconnectionReason reason) {
  // Only log a single disconnection reason per connection attempt. Edge cases
  // can cause this function to be invoked multiple times.
  if (!has_disconnect_reason_been_logged_) {
    has_disconnect_reason_been_logged_ = true;
    util::RecordNearbyDisconnection(reason);
  }

  if (!has_recorded_no_webrtc_metric_ && !has_upgraded_to_webrtc_ &&
      !time_when_connection_accepted_.is_null() &&
      (base::Time::Now() - time_when_connection_accepted_) <
          kWebRtcUpgradeDelay) {
    has_recorded_no_webrtc_metric_ = true;
    RecordConnectionMediumMetric(
        ConnectionMedium::kDisconnectedInUnder30Seconds);
  }

  if (!need_to_disconnect_endpoint_) {
    TransitionToDisconnectedAndInvokeCallback();
    return;
  }

  if (connection_status_ == ConnectionStatus::kDisconnecting)
    return;

  TransitionToStatus(ConnectionStatus::kDisconnecting);
  nearby_connections_->DisconnectFromEndpoint(
      mojom::kServiceId, remote_endpoint_id_,
      base::BindOnce(
          &NearbyConnectionBrokerImpl::OnDisconnectFromEndpointResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnectionBrokerImpl::TransitionToDisconnectedAndInvokeCallback() {
  if (connection_status_ == ConnectionStatus::kDisconnected)
    return;

  TransitionToStatus(ConnectionStatus::kDisconnected);
  InvokeDisconnectedCallback();
}

void NearbyConnectionBrokerImpl::OnEndpointDiscovered(
    const std::string& endpoint_id,
    DiscoveredEndpointInfoPtr info) {
  DCHECK_EQ(ConnectionStatus::kDiscoveringEndpoint, connection_status_);

  DCHECK(!endpoint_id.empty());
  remote_endpoint_id_ = endpoint_id;
  TransitionToStatus(ConnectionStatus::kRequestingConnection);

  nearby_connections_->RequestConnection(
      mojom::kServiceId, info->endpoint_info, remote_endpoint_id_,
      ConnectionOptions::New(MediumSelection::New(/*bluetooth=*/true,
                                                  /*ble=*/false,
                                                  /*webrtc=*/true,
                                                  /*wifi_lan=*/false),
                             /*remote_bluetooth_mac_address=*/base::nullopt),
      connection_lifecycle_listener_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnRequestConnectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnectionBrokerImpl::OnDiscoveryFailure() {
  DCHECK_EQ(ConnectionStatus::kDiscoveringEndpoint, connection_status_);
  Disconnect(util::NearbyDisconnectionReason::kFailedDiscovery);
}

void NearbyConnectionBrokerImpl::OnRequestConnectionResult(Status status) {
  util::RecordRequestConnectionResult(status);

  // In the success case, OnConnectionInitiated() is expected to be called to
  // continue the flow, so nothing else needs to be done in this callback.
  if (status == Status::kSuccess)
    return;

  PA_LOG(WARNING) << "RequestConnection() failed: " << status;
  Disconnect(util::NearbyDisconnectionReason::kFailedRequestingConnection);
}

void NearbyConnectionBrokerImpl::OnAcceptConnectionResult(Status status) {
  util::RecordAcceptConnectionResult(status);

  if (status == Status::kSuccess) {
    // It is possible that by the time OnAcceptConnectionResult() is invoked,
    // we have already passed the kAcceptingConnection (e.g., if the connection
    // was already accepted). To ensure we don't accidentally disconnect from a
    // valid connection, only transition to
    // kWaitingForConnectionToBeAcceptedByRemoteDevice if we are still accepting
    // the connection. See https://crbug.com/1175489 for details.
    if (connection_status_ == ConnectionStatus::kAcceptingConnection) {
      TransitionToStatus(
          ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice);
    }
    return;
  }

  PA_LOG(WARNING) << "AcceptConnection() failed: " << status;
  Disconnect(util::NearbyDisconnectionReason::kFailedAcceptingConnection);
}

void NearbyConnectionBrokerImpl::OnSendPayloadResult(
    SendMessageCallback callback,
    Status status) {
  util::RecordSendPayloadResult(status);

  bool success = status == Status::kSuccess;
  std::move(callback).Run(success);

  base::UmaHistogramBoolean(
      "MultiDevice.SecureChannel.Nearby.SendMessageResult", success);

  if (success)
    return;

  PA_LOG(WARNING) << "OnSendPayloadResult() failed: " << status;
  Disconnect(util::NearbyDisconnectionReason::kSendMessageFailed);
}

void NearbyConnectionBrokerImpl::OnDisconnectFromEndpointResult(Status status) {
  util::RecordDisconnectFromEndpointResult(status);

  // If the disconnection was successful, wait for the OnDisconnected()
  // callback.
  if (status == Status::kSuccess)
    return;

  PA_LOG(WARNING) << "Failed to disconnect from endpoint with ID "
                  << remote_endpoint_id_ << ": " << status;
  need_to_disconnect_endpoint_ = false;
  Disconnect(util::NearbyDisconnectionReason::kDisconnectionRequestedByClient);
}

void NearbyConnectionBrokerImpl::OnConnectionStatusChangeTimeout() {
  if (connection_status_ == ConnectionStatus::kDisconnecting) {
    PA_LOG(WARNING) << "Timeout disconnecting from endpoint";
    TransitionToDisconnectedAndInvokeCallback();
    return;
  }

  // If there is a timeout requesting a connection, we should still try to
  // disconnect from the endpoint in case the endpoint was almost about to be
  // connected before the timeout occurred.
  if (connection_status_ == ConnectionStatus::kRequestingConnection)
    need_to_disconnect_endpoint_ = true;

  PA_LOG(WARNING) << "Timeout changing connection status";
  util::NearbyDisconnectionReason reason;
  switch (connection_status_) {
    case ConnectionStatus::kDiscoveringEndpoint:
      reason = util::NearbyDisconnectionReason::kTimeoutDuringDiscovery;
      break;
    case ConnectionStatus::kRequestingConnection:
      reason = util::NearbyDisconnectionReason::kTimeoutDuringRequestConnection;
      break;
    case ConnectionStatus::kAcceptingConnection:
      reason = util::NearbyDisconnectionReason::kTimeoutDuringAcceptConnection;
      break;
    case ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice:
      reason =
          util::NearbyDisconnectionReason::kTimeoutWaitingForConnectionAccepted;
      break;
    default:
      NOTREACHED() << "Unexpected timeout with connection status "
                   << connection_status_;
      reason = util::NearbyDisconnectionReason::kConnectionLost;
      break;
  }
  Disconnect(reason);
}

void NearbyConnectionBrokerImpl::OnMojoDisconnection() {
  PA_LOG(INFO) << __func__;

  // If there is a mojo disconnect while requesting a connection, we should
  // still try to disconnect from the endpoint in case the endpoint was almost
  // about to be connected.
  if (connection_status_ == ConnectionStatus::kRequestingConnection)
    need_to_disconnect_endpoint_ = true;

  Disconnect(util::NearbyDisconnectionReason::kDisconnectionRequestedByClient);
}

void NearbyConnectionBrokerImpl::SendMessage(const std::string& message,
                                             SendMessageCallback callback) {
  DCHECK_EQ(ConnectionStatus::kConnected, connection_status_);

  std::vector<uint8_t> message_as_bytes(message.begin(), message.end());

  // Randomly generate a new payload ID for each message sent. Each payload is
  // expected to have its own ID, so we randomly generate one each time instead
  // of starting from 0 for each NearbyConnectionBrokerImpl instance. Note that
  // payloads are only shared between two devices, so the chance of a collision
  // in a 64-bit value is negligible.
  uint64_t unsigned_payload_id = base::RandUint64();

  // Interpret |unsigned_payload_id|'s bytes as a signed value for use in the
  // SendPayload() API.
  const int64_t* payload_id_ptr =
      reinterpret_cast<const int64_t*>(&unsigned_payload_id);

  nearby_connections_->SendPayload(
      mojom::kServiceId, std::vector<std::string>{remote_endpoint_id_},
      Payload::New(*payload_id_ptr, PayloadContent::NewBytes(
                                        BytesPayload::New(message_as_bytes))),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnSendPayloadResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  util::LogMessageAction(util::MessageAction::kMessageSent);
}

void NearbyConnectionBrokerImpl::OnConnectionInitiated(
    const std::string& endpoint_id,
    ConnectionInfoPtr info) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnConnectionInitiated(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  // Ignore in the event we are currently disconnecting. Either
  // OnConnectionRejected or OnDisconnected will be called eventually.
  if (connection_status_ == ConnectionStatus::kDisconnecting)
    return;

  DCHECK_EQ(ConnectionStatus::kRequestingConnection, connection_status_);
  TransitionToStatus(ConnectionStatus::kAcceptingConnection);
  need_to_disconnect_endpoint_ = true;

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

  DCHECK(connection_status_ == ConnectionStatus::kAcceptingConnection ||
         connection_status_ ==
             ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice);
  TransitionToStatus(ConnectionStatus::kConnected);
  RecordConnectionMediumMetric(ConnectionMedium::kConnectedViaBluetooth);
  time_when_connection_accepted_ = base::Time::Now();

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

  if (connection_status_ == ConnectionStatus::kDisconnecting) {
    // If this callback is invoked while we are disconnecting, we can consider
    // the disconnect successful.
    need_to_disconnect_endpoint_ = false;
    Disconnect(
        util::NearbyDisconnectionReason::kDisconnectionRequestedByClient);
    return;
  }

  PA_LOG(WARNING) << "Connection rejected: " << status;
  Disconnect(util::NearbyDisconnectionReason::kConnectionRejected);
}

void NearbyConnectionBrokerImpl::OnDisconnected(
    const std::string& endpoint_id) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnDisconnected(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  if (connection_status_ != ConnectionStatus::kDisconnecting) {
    PA_LOG(WARNING) << "Connection disconnected unexpectedly";
  }
  need_to_disconnect_endpoint_ = false;
  Disconnect(util::NearbyDisconnectionReason::kConnectionLost);
}

void NearbyConnectionBrokerImpl::OnBandwidthChanged(
    const std::string& endpoint_id,
    Medium medium) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnBandwidthChanged(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  PA_LOG(INFO) << "Bandwidth changed: " << medium;

  if (medium == Medium::kWebRtc) {
    has_upgraded_to_webrtc_ = true;
    RecordConnectionMediumMetric(ConnectionMedium::kUpgradedToWebRtc);

    DCHECK(!time_when_connection_accepted_.is_null());
    base::TimeDelta webrtc_upgrade_duration =
        base::Time::Now() - time_when_connection_accepted_;
    RecordWebRtcUpgradeDuration(webrtc_upgrade_duration);
  }
}

void NearbyConnectionBrokerImpl::OnPayloadReceived(
    const std::string& endpoint_id,
    PayloadPtr payload) {
  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnPayloadReceived(): unexpected endpoint ID "
                    << endpoint_id;
    return;
  }

  if (!payload->content->is_bytes()) {
    PA_LOG(WARNING) << "OnPayloadReceived(): Received unexpected payload type "
                    << "(was expecting bytes type). Disconnecting.";
    Disconnect(util::NearbyDisconnectionReason::kReceivedUnexpectedPayloadType);
    return;
  }

  PA_LOG(VERBOSE) << "OnPayloadReceived(): Received message with payload ID "
                  << payload->id;
  const std::vector<uint8_t>& message_as_bytes =
      payload->content->get_bytes()->bytes;
  NotifyMessageReceived(
      std::string(message_as_bytes.begin(), message_as_bytes.end()));

  util::LogMessageAction(util::MessageAction::kMessageReceived);
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
    case NearbyConnectionBrokerImpl::ConnectionStatus::kDisconnecting:
      stream << "[Disconnecting]";
      break;
    case NearbyConnectionBrokerImpl::ConnectionStatus::kDisconnected:
      stream << "[Disconnected]";
      break;
  }
  return stream;
}

}  // namespace secure_channel
}  // namespace chromeos

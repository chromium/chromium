// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_connection_broker_impl.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder.h"
#include "chrome/browser/ash/secure_channel/util/histogram_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace secure_channel {
namespace {

using ::nearby::connections::mojom::BytesPayload;
using ::nearby::connections::mojom::ConnectionInfoPtr;
using ::nearby::connections::mojom::ConnectionOptions;
using ::nearby::connections::mojom::DiscoveredEndpointInfoPtr;
using ::nearby::connections::mojom::Medium;
using ::nearby::connections::mojom::MediumSelection;
using ::nearby::connections::mojom::NearbyConnections;
using ::nearby::connections::mojom::Payload;
using ::nearby::connections::mojom::PayloadContent;
using ::nearby::connections::mojom::PayloadPtr;
using ::nearby::connections::mojom::PayloadStatus;
using ::nearby::connections::mojom::PayloadTransferUpdatePtr;
using ::nearby::connections::mojom::Status;

NearbyConnectionBrokerImpl::Factory* g_test_factory = nullptr;

constexpr base::TimeDelta kConnectionStatusChangeTimeout = base::Seconds(10);

// The amount of time by which we can expect a WebRTC upgrade to have been
// completed. According to metrics, 30 seconds is the 95th+ percentile of how
// long it takes to upgrade to WebRTC.
constexpr base::TimeDelta kWebRtcUpgradeDelay = base::Seconds(30);

// These values are set to help with Phone Hub battery drain (see: b/183505430)
// by making the Nearby Connections layer 'keep alive' ping and activity timeout
// longer. There are additional values at the WebRTC peer connection layer that
// are independent of these values and both must also be set longer than the
// defaults for battery life to improve. These two layers  (Nearby Connections
// and WebRtc) do not have to be synced explicitly, but the shortest interval
// will drive the number of kernel wakeups that cause the battery drain on the
// phone side. Both layers produce their own pings/keep alive messages so Nearby
// Connections is not responsible for sending data to keep the WebRTC layer
// alive. If these values need to be tweaked, make sure to run a power analysis
// for Phone battery drain with a persistent Phone Hub connection and understand
// the impact.
//
// Nearby Connections keep alive interval default is 5 seconds.
constexpr base::TimeDelta kKeepAliveInterval = base::Seconds(25);
// Nearby Connections keep alive timeout default is 30 seconds. When the phone
// goes into deep sleep mode Chrome OS cannot expect to receive keepalives every
// 25 seconds. The timeout needs to be set high enough to ensure a stable
// connection when the phone is in deep sleep mode.
constexpr base::TimeDelta kKeepAliveTimeout = base::Minutes(10);

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
      /*min=*/base::Seconds(1),
      /*max=*/base::Minutes(5),
      /*buckets=*/50);
}

scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

mojom::NearbyConnectionStepResult ConvertStatusToStepResult(Status status) {
  switch (status) {
    case Status::kError:
      return mojom::NearbyConnectionStepResult::kError;
    case Status::kOutOfOrderApiCall:
      return mojom::NearbyConnectionStepResult::kOutOfOrderApiCall;
    case Status::kAlreadyHaveActiveStrategy:
      return mojom::NearbyConnectionStepResult::kAlreadyHaveActiveStrategy;
    case Status::kAlreadyAdvertising:
      return mojom::NearbyConnectionStepResult::kAlreadyAdvertising;
    case Status::kAlreadyDiscovering:
      return mojom::NearbyConnectionStepResult::kAlreadyDiscovering;
    case Status::kEndpointIOError:
      return mojom::NearbyConnectionStepResult::kEndpointIOError;
    case Status::kEndpointUnknown:
      return mojom::NearbyConnectionStepResult::kEndpointUnknown;
    case Status::kConnectionRejected:
      return mojom::NearbyConnectionStepResult::kConnectionRejected;
    case Status::kAlreadyConnectedToEndpoint:
      return mojom::NearbyConnectionStepResult::kAlreadyConnectedToEndpoint;
    case Status::kNotConnectedToEndpoint:
      return mojom::NearbyConnectionStepResult::kNotConnectedToEndpoint;
    case Status::kBluetoothError:
      return mojom::NearbyConnectionStepResult::kBluetoothError;
    case Status::kBleError:
      return mojom::NearbyConnectionStepResult::kBleError;
    case Status::kWifiLanError:
      return mojom::NearbyConnectionStepResult::kWifiLanError;
    case Status::kPayloadUnknown:
      return mojom::NearbyConnectionStepResult::kPayloadUnknown;
    case Status::kAlreadyListening:
      return mojom::NearbyConnectionStepResult::kAlreadyAdvertising;
    case Status::kReset:
      return mojom::NearbyConnectionStepResult::kReset;
    case Status::kTimeout:
      return mojom::NearbyConnectionStepResult::kTimeout;
    case Status::kUnknown:
      return mojom::NearbyConnectionStepResult::kUnknown;
    case Status::kSuccess:
      return mojom::NearbyConnectionStepResult::kSuccess;
    case Status::kNextValue:
      NOTREACHED_IN_MIGRATION();
      return mojom::NearbyConnectionStepResult::kMaxValue;
  }
}

}  // namespace

// static
std::unique_ptr<NearbyConnectionBroker>
NearbyConnectionBrokerImpl::Factory::Create(
    const std::vector<uint8_t>& bluetooth_public_address,
    const std::vector<uint8_t>& eid,
    NearbyEndpointFinder* endpoint_finder,
    mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
    mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
        file_payload_handler_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener,
    const mojo::SharedRemote<NearbyConnections>& nearby_connections,
    base::OnceClosure on_connected_callback,
    base::OnceClosure on_disconnected_callback,
    std::unique_ptr<base::OneShotTimer> timer) {
  if (g_test_factory) {
    return g_test_factory->CreateInstance(
        bluetooth_public_address, endpoint_finder,
        std::move(message_sender_receiver),
        std::move(file_payload_handler_receiver),
        std::move(message_receiver_remote),
        std::move(nearby_connection_state_listener), nearby_connections,
        std::move(on_connected_callback), std::move(on_disconnected_callback),
        std::move(timer));
  }

  return base::WrapUnique(new NearbyConnectionBrokerImpl(
      bluetooth_public_address, eid, endpoint_finder,
      std::move(message_sender_receiver),
      std::move(file_payload_handler_receiver),
      std::move(message_receiver_remote),
      std::move(nearby_connection_state_listener), nearby_connections,
      std::move(on_connected_callback), std::move(on_disconnected_callback),
      std::move(timer)));
}

// static
void NearbyConnectionBrokerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

NearbyConnectionBrokerImpl::NearbyConnectionBrokerImpl(
    const std::vector<uint8_t>& bluetooth_public_address,
    const std::vector<uint8_t>& eid,
    NearbyEndpointFinder* endpoint_finder,
    mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
    mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
        file_payload_handler_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener,
    const mojo::SharedRemote<NearbyConnections>& nearby_connections,
    base::OnceClosure on_connected_callback,
    base::OnceClosure on_disconnected_callback,
    std::unique_ptr<base::OneShotTimer> timer)
    : NearbyConnectionBroker(bluetooth_public_address,
                             std::move(message_sender_receiver),
                             std::move(file_payload_handler_receiver),
                             std::move(message_receiver_remote),
                             std::move(nearby_connection_state_listener),
                             std::move(on_connected_callback),
                             std::move(on_disconnected_callback)),
      endpoint_finder_(endpoint_finder),
      nearby_connections_(nearby_connections),
      timer_(std::move(timer)),
      task_runner_(CreateTaskRunner()) {
  TransitionToStatus(ConnectionStatus::kDiscoveringEndpoint);
  NotifyConnectionStateChanged(
      mojom::NearbyConnectionStep::kDiscoveringEndpointStarted,
      mojom::NearbyConnectionStepResult::kSuccess);
  endpoint_finder_->FindEndpoint(
      bluetooth_public_address, eid,
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

  if (connection_status_ == ConnectionStatus::kDisconnecting) {
    return;
  }

  TransitionToStatus(ConnectionStatus::kDisconnecting);
  NotifyConnectionStateChanged(
      mojom::NearbyConnectionStep::kDisconnectionStarted,
      mojom::NearbyConnectionStepResult::kSuccess);
  nearby_connections_->DisconnectFromEndpoint(
      mojom::kServiceId, remote_endpoint_id_,
      base::BindOnce(
          &NearbyConnectionBrokerImpl::OnDisconnectFromEndpointResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnectionBrokerImpl::TransitionToDisconnectedAndInvokeCallback() {
  if (connection_status_ == ConnectionStatus::kDisconnected) {
    return;
  }

  TransitionToStatus(ConnectionStatus::kDisconnected);
  NotifyConnectionStateChanged(
      mojom::NearbyConnectionStep::kDisconnectionFinished,
      mojom::NearbyConnectionStepResult::kSuccess);
  CleanUpPendingFileTransfers();
  InvokeDisconnectedCallback();
}

void NearbyConnectionBrokerImpl::OnEndpointDiscovered(
    const std::string& endpoint_id,
    DiscoveredEndpointInfoPtr info) {
  DCHECK_EQ(ConnectionStatus::kDiscoveringEndpoint, connection_status_);
  NotifyConnectionStateChanged(
      mojom::NearbyConnectionStep::kDiscoveringEndpointEnded,
      mojom::NearbyConnectionStepResult::kSuccess);

  DCHECK(!endpoint_id.empty());
  remote_endpoint_id_ = endpoint_id;
  TransitionToStatus(ConnectionStatus::kRequestingConnection);
  NotifyConnectionStateChanged(
      mojom::NearbyConnectionStep::kRequestingConnectionStarted,
      mojom::NearbyConnectionStepResult::kSuccess);

  nearby_connections_->RequestConnection(
      mojom::kServiceId, info->endpoint_info, remote_endpoint_id_,
      ConnectionOptions::New(MediumSelection::New(/*bluetooth=*/true,
                                                  /*ble=*/false,
                                                  /*webrtc=*/true,
                                                  /*wifi_lan=*/false,
                                                  /*wifi_direct=*/false),
                             /*remote_bluetooth_mac_address=*/std::nullopt,
                             features::IsNearbyKeepAliveFixEnabled()
                                 ? std::make_optional(kKeepAliveInterval)
                                 : std::nullopt,
                             features::IsNearbyKeepAliveFixEnabled()
                                 ? std::make_optional(kKeepAliveTimeout)
                                 : std::nullopt),
      connection_lifecycle_listener_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnRequestConnectionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyConnectionBrokerImpl::OnDiscoveryFailure(Status status) {
  DCHECK_EQ(ConnectionStatus::kDiscoveringEndpoint, connection_status_);
  NotifyConnectionStateChanged(
      mojom::NearbyConnectionStep::kDiscoveringEndpointEnded,
      ConvertStatusToStepResult(status));
  Disconnect(util::NearbyDisconnectionReason::kFailedDiscovery);
}

void NearbyConnectionBrokerImpl::OnRequestConnectionResult(Status status) {
  util::RecordRequestConnectionResult(status);

  // In the success case, OnConnectionInitiated() is expected to be called to
  // continue the flow, so nothing else needs to be done in this callback.
  if (status == Status::kSuccess) {
    NotifyConnectionStateChanged(
        mojom::NearbyConnectionStep::kRequestingConnectionEnded,
        mojom::NearbyConnectionStepResult::kSuccess);
    return;
  }

  PA_LOG(WARNING) << "RequestConnection() failed: " << status;
  if (connection_status_ != ConnectionStatus::kDisconnecting) {
    // OnDiscoveryFailure maybe invoked after OnConnectionStatusChangeTimeout.
    // In this case, we have already recorded NearbyConnectionStep as failed due
    // to timeout.
    NotifyConnectionStateChanged(
        mojom::NearbyConnectionStep::kRequestingConnectionEnded,
        ConvertStatusToStepResult(status));
  }
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
      NotifyConnectionStateChanged(
          mojom::NearbyConnectionStep::kAcceptingConnectionFinished,
          mojom::NearbyConnectionStepResult::kSuccess);
      TransitionToStatus(
          ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice);
      NotifyConnectionStateChanged(
          mojom::NearbyConnectionStep::
              kWaitingForConnectionToBeAcceptedByRemoteDeviceStarted,
          mojom::NearbyConnectionStepResult::kSuccess);
    }
    return;
  }

  PA_LOG(WARNING) << "AcceptConnection() failed: " << status;
  if (connection_status_ == ConnectionStatus::kAcceptingConnection) {
    NotifyConnectionStateChanged(
        mojom::NearbyConnectionStep::kAcceptingConnectionFinished,
        ConvertStatusToStepResult(status));
  }
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
  if (connection_status_ == ConnectionStatus::kRequestingConnection) {
    need_to_disconnect_endpoint_ = true;
  }

  PA_LOG(WARNING) << "Timeout changing connection status";
  util::NearbyDisconnectionReason reason;
  mojom::NearbyConnectionStep connection_step;
  switch (connection_status_) {
    case ConnectionStatus::kDiscoveringEndpoint:
      reason = util::NearbyDisconnectionReason::kTimeoutDuringDiscovery;
      connection_step = mojom::NearbyConnectionStep::kDiscoveringEndpointEnded;
      break;
    case ConnectionStatus::kRequestingConnection:
      reason = util::NearbyDisconnectionReason::kTimeoutDuringRequestConnection;
      connection_step = mojom::NearbyConnectionStep::kRequestingConnectionEnded;
      break;
    case ConnectionStatus::kAcceptingConnection:
      reason = util::NearbyDisconnectionReason::kTimeoutDuringAcceptConnection;
      connection_step =
          mojom::NearbyConnectionStep::kAcceptingConnectionFinished;
      break;
    case ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice:
      reason =
          util::NearbyDisconnectionReason::kTimeoutWaitingForConnectionAccepted;
      connection_step = mojom::NearbyConnectionStep::
          kWaitingForConnectionToBeAcceptedByRemoteDeviceEnded;
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected timeout with connection status " << connection_status_;
      reason = util::NearbyDisconnectionReason::kConnectionLost;
      connection_step = mojom::NearbyConnectionStep::kDisconnectionFinished;
      break;
  }
  NotifyConnectionStateChanged(
      connection_step,
      mojom::NearbyConnectionStepResult::kTimeoutTransitionState);
  Disconnect(reason);
}

void NearbyConnectionBrokerImpl::OnMojoDisconnection() {
  PA_LOG(INFO) << __func__;

  // If there is a mojo disconnect while requesting a connection, we should
  // still try to disconnect from the endpoint in case the endpoint was almost
  // about to be connected.
  if (connection_status_ == ConnectionStatus::kRequestingConnection) {
    need_to_disconnect_endpoint_ = true;
  }

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

void NearbyConnectionBrokerImpl::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    mojo::PendingRemote<mojom::FilePayloadListener> listener,
    RegisterPayloadFileCallback callback) {
  nearby_connections_->RegisterPayloadFile(
      mojom::kServiceId, payload_id, std::move(payload_files->input_file),
      std::move(payload_files->output_file),
      base::BindOnce(&NearbyConnectionBrokerImpl::OnPayloadFileRegistered,
                     weak_ptr_factory_.GetWeakPtr(), payload_id,
                     std::move(listener), std::move(callback)));
}

void NearbyConnectionBrokerImpl::OnPayloadFileRegistered(
    int64_t payload_id,
    mojo::PendingRemote<mojom::FilePayloadListener> listener,
    RegisterPayloadFileCallback callback,
    Status status) {
  bool success = status == Status::kSuccess;
  if (success) {
    mojo::Remote<mojom::FilePayloadListener> listener_remote(
        std::move(listener));
    // Safe to use Unretained because the Remote and its disconnect handler does
    // not out live NearbyConnectionBrokerImpl.
    listener_remote.set_disconnect_handler(base::BindOnce(
        &NearbyConnectionBrokerImpl::OnFilePayloadListenerDisconnect,
        base::Unretained(this), payload_id));
    file_payload_listeners_.emplace(payload_id, std::move(listener_remote));
  }
  std::move(callback).Run(success);
  util::RecordRegisterPayloadFilesResult(status);
}

void NearbyConnectionBrokerImpl::OnFilePayloadListenerDisconnect(
    int64_t payload_id) {
  file_payload_listeners_.erase(payload_id);
}

void NearbyConnectionBrokerImpl::CleanUpPendingFileTransfers() {
  for (auto& id_to_listener : file_payload_listeners_) {
    id_to_listener.second->OnFileTransferUpdate(mojom::FileTransferUpdate::New(
        id_to_listener.first, mojom::FileTransferStatus::kCanceled,
        /*total_bytes=*/0,
        /*bytes_transferred=*/0));
    util::LogFileTransferResult(
        util::FileTransferResult::kFileTransferCanceled);
  }
  file_payload_listeners_.clear();
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
  if (connection_status_ == ConnectionStatus::kDisconnecting) {
    return;
  }

  DCHECK_EQ(ConnectionStatus::kRequestingConnection, connection_status_);
  TransitionToStatus(ConnectionStatus::kAcceptingConnection);
  NotifyConnectionStateChanged(
      mojom::NearbyConnectionStep::kAcceptingConnectionStarted,
      mojom::NearbyConnectionStepResult::kSuccess);
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
  if (connection_status_ == ConnectionStatus::kAcceptingConnection) {
    NotifyConnectionStateChanged(
        mojom::NearbyConnectionStep::kAcceptingConnectionFinished,
        mojom::NearbyConnectionStepResult::kSuccess);
  } else if (connection_status_ ==
             ConnectionStatus::
                 kWaitingForConnectionToBeAcceptedByRemoteDevice) {
    NotifyConnectionStateChanged(
        mojom::NearbyConnectionStep::
            kWaitingForConnectionToBeAcceptedByRemoteDeviceEnded,
        mojom::NearbyConnectionStepResult::kSuccess);
  }
  TransitionToStatus(ConnectionStatus::kConnected);
  NotifyConnectionStateChanged(mojom::NearbyConnectionStep::kConnected,
                               mojom::NearbyConnectionStepResult::kSuccess);
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
  mojom::NearbyConnectionStep connection_step;
  switch (connection_status_) {
    case ConnectionStatus::kDiscoveringEndpoint:
      connection_step = mojom::NearbyConnectionStep::kDiscoveringEndpointEnded;
      break;
    case ConnectionStatus::kRequestingConnection:
      connection_step = mojom::NearbyConnectionStep::kRequestingConnectionEnded;
      break;
    case ConnectionStatus::kAcceptingConnection:
      connection_step =
          mojom::NearbyConnectionStep::kAcceptingConnectionFinished;
      break;
    case ConnectionStatus::kWaitingForConnectionToBeAcceptedByRemoteDevice:
      connection_step = mojom::NearbyConnectionStep::
          kWaitingForConnectionToBeAcceptedByRemoteDeviceEnded;
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected connection status when connection rejected"
          << connection_status_;
      connection_step = mojom::NearbyConnectionStep::kDiscoveringEndpointEnded;
  }
  NotifyConnectionStateChanged(
      connection_step, mojom::NearbyConnectionStepResult::kConnectionRejected);
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
    NotifyConnectionStateChanged(mojom::NearbyConnectionStep::kUpgradedToWebRtc,
                                 mojom::NearbyConnectionStepResult::kSuccess);
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

  if (payload->content->is_bytes()) {
    PA_LOG(VERBOSE) << "OnPayloadReceived(): Received message with payload ID "
                    << payload->id;
    const std::vector<uint8_t>& message_as_bytes =
        payload->content->get_bytes()->bytes;
    NotifyMessageReceived(
        std::string(message_as_bytes.begin(), message_as_bytes.end()));

    util::LogMessageAction(util::MessageAction::kMessageReceived);
  } else if (ash::features::IsPhoneHubCameraRollEnabled() &&
             payload->content->is_file()) {
    if (!file_payload_listeners_.contains(payload->id)) {
      PA_LOG(WARNING)
          << "OnPayloadReceived(): Received unregistered file payload with ID "
          << payload->id << ". Disconnecting.";
      util::LogFileAction(util::FileAction::kUnexpectedFileReceived);
      Disconnect(
          util::NearbyDisconnectionReason::kReceivedUnregisteredFilePayload);
    } else {
      PA_LOG(VERBOSE) << "OnPayloadReceived(): Received file with payload ID "
                      << payload->id;
      util::LogFileAction(util::FileAction::kRegisteredFileReceived);
    }

    // We don't need to use the base::File provided by |payload| and it should
    // be closed in a task that may block. Otherwise the file will be closed on
    // the current thread when |payload| goes out of scope, which would result
    // in a DCHECK failure because base::File::Close() is a blocking call.
    task_runner_->DeleteSoon(
        FROM_HERE, std::make_unique<base::File>(
                       std::move(payload->content->get_file()->file)));
  } else {
    PA_LOG(WARNING) << "OnPayloadReceived(): Received unexpected payload type "
                    << "(was expecting bytes type). Disconnecting.";
    Disconnect(util::NearbyDisconnectionReason::kReceivedUnexpectedPayloadType);
  }
}

mojom::FileTransferStatus ConvertFileTransferStatus(PayloadStatus status) {
  switch (status) {
    case PayloadStatus::kSuccess:
      return mojom::FileTransferStatus::kSuccess;
    case PayloadStatus::kFailure:
      return mojom::FileTransferStatus::kFailure;
    case PayloadStatus::kInProgress:
      return mojom::FileTransferStatus::kInProgress;
    case PayloadStatus::kCanceled:
      return mojom::FileTransferStatus::kCanceled;
  }
}

void NearbyConnectionBrokerImpl::OnPayloadTransferUpdate(
    const std::string& endpoint_id,
    ::nearby::connections::mojom::PayloadTransferUpdatePtr update) {
  if (!ash::features::IsPhoneHubCameraRollEnabled()) {
    return;
  }

  if (remote_endpoint_id_ != endpoint_id) {
    PA_LOG(WARNING) << "OnPayloadTransferUpdate(): unexpected endpoint ID; "
                    << "expected=" << endpoint_id
                    << ", actual=" << remote_endpoint_id_;
    return;
  }

  auto it = file_payload_listeners_.find(update->payload_id);
  if (it == file_payload_listeners_.end()) {
    return;
  }

  PA_LOG(VERBOSE)
      << "OnPayloadTransferUpdate(): Received update for file payload "
      << update->payload_id;

  it->second->OnFileTransferUpdate(mojom::FileTransferUpdate::New(
      update->payload_id, ConvertFileTransferStatus(update->status),
      update->total_bytes, update->bytes_transferred));

  bool is_transfer_complete = false;
  switch (update->status) {
    case PayloadStatus::kInProgress:
      return;
    case PayloadStatus::kSuccess:
      is_transfer_complete = true;
      util::LogFileTransferResult(
          util::FileTransferResult::kFileTransferSuccess);
      break;
    case PayloadStatus::kFailure:
      is_transfer_complete = true;
      util::LogFileTransferResult(
          util::FileTransferResult::kFileTransferFailure);
      break;
    case PayloadStatus::kCanceled:
      is_transfer_complete = true;
      util::LogFileTransferResult(
          util::FileTransferResult::kFileTransferCanceled);
      break;
  }
  if (is_transfer_complete) {
    file_payload_listeners_.erase(it);
  }
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
}  // namespace ash

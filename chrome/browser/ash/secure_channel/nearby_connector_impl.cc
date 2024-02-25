// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_connector_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/secure_channel/nearby_connection_broker_impl.h"
#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder_impl.h"
#include "chrome/browser/ash/secure_channel/util/histogram_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {
namespace secure_channel {

using NearbyProcessShutdownReason =
    nearby::NearbyProcessManager::NearbyProcessShutdownReason;

NearbyConnectorImpl::ConnectionRequestMetadata::ConnectionRequestMetadata(
    const std::vector<uint8_t>& bluetooth_public_address,
    const std::vector<uint8_t>& eid,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener,
    ConnectCallback callback)
    : bluetooth_public_address(bluetooth_public_address),
      eid(eid),
      message_receiver(std::move(message_receiver)),
      nearby_connection_state_listener(
          std::move(nearby_connection_state_listener)),
      callback(std::move(callback)) {}

NearbyConnectorImpl::ConnectionRequestMetadata::~ConnectionRequestMetadata() =
    default;

NearbyConnectorImpl::ActiveConnectionAttempt::ActiveConnectionAttempt(
    const base::UnguessableToken& attempt_id,
    std::unique_ptr<NearbyEndpointFinder> endpoint_finder,
    ConnectCallback callback)
    : attempt_id(attempt_id),
      endpoint_finder(std::move(endpoint_finder)),
      callback(std::move(callback)) {}

NearbyConnectorImpl::ActiveConnectionAttempt::~ActiveConnectionAttempt() =
    default;

NearbyConnectorImpl::NearbyConnectorImpl(
    nearby::NearbyProcessManager* nearby_process_manager)
    : nearby_process_manager_(nearby_process_manager) {}

NearbyConnectorImpl::~NearbyConnectorImpl() = default;

void NearbyConnectorImpl::Connect(
    const std::vector<uint8_t>& bluetooth_public_address,
    const std::vector<uint8_t>& eid,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener,
    ConnectCallback callback) {
  queued_connection_requests_.emplace(
      std::make_unique<ConnectionRequestMetadata>(
          bluetooth_public_address, eid, std::move(message_receiver),
          std::move(nearby_connection_state_listener), std::move(callback)));
  ProcessQueuedConnectionRequests();
}

void NearbyConnectorImpl::Shutdown() {
  nearby_process_manager_ = nullptr;
  ClearActiveAndPendingConnections();
}

void NearbyConnectorImpl::ClearActiveAndPendingConnections() {
  if (active_connection_attempt_) {
    InvokeActiveConnectionAttemptCallback(mojo::NullRemote(),
                                          mojo::NullRemote());
    active_connection_attempt_.reset();
  }
  id_to_brokers_map_.clear();
  process_reference_.reset();
}

void NearbyConnectorImpl::ProcessQueuedConnectionRequests() {
  // Shutdown() has been called, so no further connection requests should be
  // attempted.
  if (!nearby_process_manager_)
    return;

  // No queued requests; nothing to do.
  if (queued_connection_requests_.empty())
    return;

  // Only one connection can be requested at a time.
  if (active_connection_attempt_)
    return;

  // If there is no currently-held process reference, request one. This ensures
  // that the Nearby utility process is running.
  if (!process_reference_) {
    PA_LOG(VERBOSE) << "Obtaining Nearby process reference";
    process_reference_ = nearby_process_manager_->GetNearbyProcessReference(
        base::BindOnce(&NearbyConnectorImpl::OnNearbyProcessStopped,
                       weak_ptr_factory_.GetWeakPtr()));

    if (!process_reference_) {
      PA_LOG(WARNING) << "Could not obtain Nearby process reference";
      return;
    }
  }

  // Remove the request from the front of the queue.
  std::unique_ptr<ConnectionRequestMetadata> metadata =
      std::move(queued_connection_requests_.front());
  queued_connection_requests_.pop();

  auto new_broker_id = base::UnguessableToken::Create();
  mojo::PendingRemote<mojom::NearbyMessageSender> message_sender_pending_remote;
  mojo::PendingReceiver<mojom::NearbyMessageSender>
      message_sender_pending_receiver =
          message_sender_pending_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
      file_payload_handler_pending_remote;
  mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
      file_payload_handler_pending_receiver =
          file_payload_handler_pending_remote.InitWithNewPipeAndPassReceiver();

  DCHECK(!active_connection_attempt_);
  active_connection_attempt_.emplace(
      new_broker_id,
      NearbyEndpointFinderImpl::Factory::Create(
          process_reference_->GetNearbyConnections()),
      std::move(metadata->callback));

  id_to_brokers_map_[new_broker_id] =
      NearbyConnectionBrokerImpl::Factory::Create(
          metadata->bluetooth_public_address, metadata->eid,
          active_connection_attempt_->endpoint_finder.get(),
          std::move(message_sender_pending_receiver),
          std::move(file_payload_handler_pending_receiver),
          std::move(metadata->message_receiver),
          std::move(metadata->nearby_connection_state_listener),
          process_reference_->GetNearbyConnections(),
          base::BindOnce(&NearbyConnectorImpl::OnConnected,
                         base::Unretained(this), new_broker_id,
                         std::move(message_sender_pending_remote),
                         std::move(file_payload_handler_pending_remote)),
          base::BindOnce(&NearbyConnectorImpl::OnDisconnected,
                         base::Unretained(this), new_broker_id));
}

void NearbyConnectorImpl::OnNearbyProcessStopped(
    NearbyProcessShutdownReason shutdown_reason) {
  PA_LOG(WARNING) << "Nearby process stopped unexpectedly. Destroying active "
                  << "connections. Shutdown reason: " << shutdown_reason;

  RecordNearbyDisconnectionForActiveBrokers(shutdown_reason);
  ClearActiveAndPendingConnections();
  ProcessQueuedConnectionRequests();
}

void NearbyConnectorImpl::RecordNearbyDisconnectionForActiveBrokers(
    NearbyProcessShutdownReason shutdown_reason) {
  if (id_to_brokers_map_.empty())
    return;

  util::NearbyDisconnectionReason disconnection_reason;

  switch (shutdown_reason) {
    case NearbyProcessShutdownReason::kCrash:
      disconnection_reason =
          util::NearbyDisconnectionReason::kNearbyProcessCrash;
      break;

    case NearbyProcessShutdownReason::kConnectionsMojoPipeDisconnection:
    case NearbyProcessShutdownReason::kPresenceMojoPipeDisconnection:
    case NearbyProcessShutdownReason::kDecoderMojoPipeDisconnection:
      disconnection_reason =
          util::NearbyDisconnectionReason::kNearbyProcessMojoDisconnection;
      break;

    case NearbyProcessShutdownReason::kNormal:
      PA_LOG(WARNING) << "Neary process stopped normally. This is unexpected "
                         "when there are active brokers.";
      disconnection_reason =
          util::NearbyDisconnectionReason::kDisconnectionRequestedByClient;
      break;
  }

  for (size_t i = 0; i < id_to_brokers_map_.size(); ++i) {
    util::RecordNearbyDisconnection(disconnection_reason);
  }
}

void NearbyConnectorImpl::OnConnected(
    const base::UnguessableToken& id,
    mojo::PendingRemote<mojom::NearbyMessageSender>
        message_sender_pending_remote,
    mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
        file_payload_handler_remote) {
  DCHECK_EQ(active_connection_attempt_->attempt_id, id);
  InvokeActiveConnectionAttemptCallback(
      std::move(message_sender_pending_remote),
      std::move(file_payload_handler_remote));
  active_connection_attempt_.reset();
  ProcessQueuedConnectionRequests();
}

void NearbyConnectorImpl::OnDisconnected(const base::UnguessableToken& id) {
  // If the pending connection could not complete, invoke the callback with an
  // unbound PendingRemote.
  if (active_connection_attempt_ &&
      active_connection_attempt_->attempt_id == id) {
    InvokeActiveConnectionAttemptCallback(mojo::NullRemote(),
                                          mojo::NullRemote());
    active_connection_attempt_.reset();
  }

  id_to_brokers_map_.erase(id);

  // If this disconnection corresponds to the last active broker, release the
  // process reference so that the Nearby utility process can shut down if
  // applicable.
  if (id_to_brokers_map_.empty()) {
    PA_LOG(VERBOSE) << "Releasing Nearby process reference";
    process_reference_.reset();
  }

  ProcessQueuedConnectionRequests();
}

void NearbyConnectorImpl::InvokeActiveConnectionAttemptCallback(
    mojo::PendingRemote<mojom::NearbyMessageSender>
        message_sender_pending_remote,
    mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
        file_payload_handler_remote) {
  std::move(active_connection_attempt_->callback)
      .Run(std::move(message_sender_pending_remote),
           std::move(file_payload_handler_remote));
}

}  // namespace secure_channel
}  // namespace ash

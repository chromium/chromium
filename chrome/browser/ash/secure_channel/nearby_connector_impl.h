// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_IMPL_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_connector.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {
namespace secure_channel {

class NearbyConnectionBroker;
class NearbyEndpointFinder;

// NearbyConnector implementation which uses NearbyProcessManager to interact
// with Nearby Connections. When a connection is requested, NearbyConnectorImpl
// obtains a reference to the Nearby utility process, and when no requests are
// active, the class releases its reference.
//
// Because only one NearbyEndpointFinder is meant to be used at a time,
// connections are connected in a queue; once one connection has been
// established, it is possible for a new one to be requested. The class
// assigns an ID to each connection and holds a reference to a
// NearbyConnectionBroker for each one.
class NearbyConnectorImpl : public NearbyConnector, public KeyedService {
 public:
  explicit NearbyConnectorImpl(
      nearby::NearbyProcessManager* nearby_process_manager);
  ~NearbyConnectorImpl() override;

 private:
  struct ConnectionRequestMetadata {
    ConnectionRequestMetadata(
        const std::vector<uint8_t>& bluetooth_public_address,
        const std::vector<uint8_t>& eid,
        mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
        mojo::PendingRemote<mojom::NearbyConnectionStateListener>
            nearby_connection_state_listener,
        NearbyConnector::ConnectCallback callback);
    ConnectionRequestMetadata(const ConnectionRequestMetadata&) = delete;
    ConnectionRequestMetadata& operator=(const ConnectionRequestMetadata&) =
        delete;
    ~ConnectionRequestMetadata();

    std::vector<uint8_t> bluetooth_public_address;
    std::vector<uint8_t> eid;
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver;
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener;
    NearbyConnector::ConnectCallback callback;
  };

  struct ActiveConnectionAttempt {
    ActiveConnectionAttempt(
        const base::UnguessableToken& attempt_id,
        std::unique_ptr<NearbyEndpointFinder> endpoint_finder,
        NearbyConnector::ConnectCallback callback);
    ~ActiveConnectionAttempt();

    base::UnguessableToken attempt_id;
    std::unique_ptr<NearbyEndpointFinder> endpoint_finder;
    NearbyConnector::ConnectCallback callback;
  };

  /// mojom::NearbyConnector:
  void Connect(
      const std::vector<uint8_t>& bluetooth_public_address,
      const std::vector<uint8_t>& eid,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
      mojo::PendingRemote<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener,
      NearbyConnector::ConnectCallback callback) override;

  // KeyedService:
  void Shutdown() override;

  void ClearActiveAndPendingConnections();
  void ProcessQueuedConnectionRequests();

  void OnNearbyProcessStopped(
      nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  void RecordNearbyDisconnectionForActiveBrokers(
      nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  void OnConnected(const base::UnguessableToken& id,
                   mojo::PendingRemote<mojom::NearbyMessageSender>
                       message_sender_pending_remote,
                   mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
                       file_payload_handler_remote);
  void OnDisconnected(const base::UnguessableToken& id);
  void InvokeActiveConnectionAttemptCallback(
      mojo::PendingRemote<mojom::NearbyMessageSender>
          message_sender_pending_remote,
      mojo::PendingRemote<mojom::NearbyFilePayloadHandler>
          file_payload_handler_remote);

  raw_ptr<nearby::NearbyProcessManager> nearby_process_manager_;

  // Reference to the Nearby utility process; null if we have not requested a
  // connection to the process (i.e., when there are no active connection
  // requests).
  std::unique_ptr<nearby::NearbyProcessManager::NearbyProcessReference>
      process_reference_;

  // Metadata for connection requests which are planned but not yet started.
  base::queue<std::unique_ptr<ConnectionRequestMetadata>>
      queued_connection_requests_;

  // Active connection brokers, which delegate messages between Nearby
  // Connections and SecureChannel. This map can contain at most one broker
  // which is in the process of connecting and any number of active connections.
  // If a broker is currently pending a connection, its ID is stored in
  // |broker_id_pending_connection_|.
  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<NearbyConnectionBroker>>
      id_to_brokers_map_;

  // Metadata for an ongoing connection attempt. If this field is set, it means
  // that the entry in |id_to_brokers_map_| with the given ID is currently
  // attempting a connection. If null, there is no pending connection attempt.
  std::optional<ActiveConnectionAttempt> active_connection_attempt_;

  base::WeakPtrFactory<NearbyConnectorImpl> weak_ptr_factory_{this};
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTOR_IMPL_H_

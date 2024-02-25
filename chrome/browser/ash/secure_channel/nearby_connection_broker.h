// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace secure_channel {

// Attempts to create a Nearby Connection to a remote device and exchange
// messages on behalf of the SecureChannel service. Implements the
// mojom::NearbyMessageSender interface so that SecureChannel can send messages
// to Nearby Connections, and uses the mojom::NearbyMessageReceiver interface to
// relay messages received from Nearby Connections back to SecureChannel.
//
// Also implements the mojom::NearbyFilePayloadHandler interface to register
// incoming file payloads with Nearby Connections.
//
// An instance of this class is only meant to be used for one connection
// request to a single device. To make a new request, create a new object.
class NearbyConnectionBroker : public mojom::NearbyMessageSender,
                               public mojom::NearbyFilePayloadHandler {
 public:
  ~NearbyConnectionBroker() override;

 protected:
  NearbyConnectionBroker(
      const std::vector<uint8_t>& bluetooth_public_address,
      mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
      mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
          file_payload_handler_receiver,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
      mojo::PendingRemote<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener_remote,
      base::OnceClosure on_connected_callback,
      base::OnceClosure on_disconnected_callback);

  const std::vector<uint8_t>& bluetooth_public_address() const {
    return bluetooth_public_address_;
  }

  // Can be overridden by derived classes to handle MessageSender,
  // FilePayloadHandler, and MessageReceiver Mojo pipes being disconnected.
  virtual void OnMojoDisconnection() {}

  void InvokeDisconnectedCallback();
  void NotifyConnected();
  void NotifyMessageReceived(const std::string& received_message);
  void NotifyConnectionStateChanged(mojom::NearbyConnectionStep step,
                                    mojom::NearbyConnectionStepResult result);

 private:
  std::vector<uint8_t> bluetooth_public_address_;
  mojo::Receiver<mojom::NearbyMessageSender> message_sender_receiver_;
  mojo::Receiver<mojom::NearbyFilePayloadHandler>
      file_payload_handler_receiver_;
  mojo::Remote<mojom::NearbyMessageReceiver> message_receiver_remote_;
  mojo::Remote<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_remote_;
  base::OnceClosure on_connected_callback_;
  base::OnceClosure on_disconnected_callback_;
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_H_

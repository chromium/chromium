// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_BROKER_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_BROKER_H_

#include "chrome/browser/ash/secure_channel/nearby_connection_broker.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"

namespace ash {
namespace secure_channel {

class FakeNearbyConnectionBroker : public NearbyConnectionBroker {
 public:
  FakeNearbyConnectionBroker(
      const std::vector<uint8_t>& bluetooth_public_address,
      mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
      mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
          file_payload_handler_receiver,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
      mojo::PendingRemote<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener,
      base::OnceClosure on_connected_callback,
      base::OnceClosure on_disconnected_callback);
  ~FakeNearbyConnectionBroker() override;

  using NearbyConnectionBroker::bluetooth_public_address;
  using NearbyConnectionBroker::InvokeDisconnectedCallback;
  using NearbyConnectionBroker::NotifyConnected;
  using NearbyConnectionBroker::NotifyMessageReceived;

  void set_should_send_message_succeed(bool should_send_message_succeed) {
    should_send_message_succeed_ = should_send_message_succeed;
  }

  const std::vector<std::string>& sent_messages() const {
    return sent_messages_;
  }

 private:
  // mojom::NearbyMessageSender:
  void SendMessage(const std::string& message,
                   SendMessageCallback callback) override;

  // mojom::NearbyFilePayloadHandler:
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      mojo::PendingRemote<mojom::FilePayloadListener> listener,
      RegisterPayloadFileCallback callback) override {}

  std::vector<std::string> sent_messages_;
  bool should_send_message_succeed_ = true;
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_BROKER_H_

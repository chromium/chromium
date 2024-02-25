// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/fake_nearby_connection_broker.h"

namespace ash {
namespace secure_channel {

FakeNearbyConnectionBroker::FakeNearbyConnectionBroker(
    const std::vector<uint8_t>& bluetooth_public_address,
    mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
    mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
        file_payload_handler_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
    mojo::PendingRemote<mojom::NearbyConnectionStateListener>
        nearby_connection_state_listener,
    base::OnceClosure on_connected_callback,
    base::OnceClosure on_disconnected_callback)
    : NearbyConnectionBroker(bluetooth_public_address,
                             std::move(message_sender_receiver),
                             std::move(file_payload_handler_receiver),
                             std::move(message_receiver_remote),
                             std::move(nearby_connection_state_listener),
                             std::move(on_connected_callback),
                             std::move(on_disconnected_callback)) {}

FakeNearbyConnectionBroker::~FakeNearbyConnectionBroker() = default;

void FakeNearbyConnectionBroker::SendMessage(const std::string& message,
                                             SendMessageCallback callback) {
  sent_messages_.push_back(message);
  std::move(callback).Run(should_send_message_succeed_);
}

}  // namespace secure_channel
}  // namespace ash

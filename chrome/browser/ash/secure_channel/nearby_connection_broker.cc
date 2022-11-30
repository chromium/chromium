// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_connection_broker.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace ash {
namespace secure_channel {

NearbyConnectionBroker::NearbyConnectionBroker(
    const std::vector<uint8_t>& bluetooth_public_address,
    mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
    mojo::PendingReceiver<mojom::NearbyFilePayloadHandler>
        file_payload_handler_receiver,
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
    base::OnceClosure on_connected_callback,
    base::OnceClosure on_disconnected_callback)
    : bluetooth_public_address_(bluetooth_public_address),
      message_sender_receiver_(this, std::move(message_sender_receiver)),
      file_payload_handler_receiver_(this,
                                     std::move(file_payload_handler_receiver)),
      message_receiver_remote_(std::move(message_receiver_remote)),
      on_connected_callback_(std::move(on_connected_callback)),
      on_disconnected_callback_(std::move(on_disconnected_callback)) {
  message_sender_receiver_.set_disconnect_handler(base::BindOnce(
      &NearbyConnectionBroker::OnMojoDisconnection, base::Unretained(this)));
  file_payload_handler_receiver_.set_disconnect_handler(base::BindOnce(
      &NearbyConnectionBroker::OnMojoDisconnection, base::Unretained(this)));
  message_receiver_remote_.set_disconnect_handler(base::BindOnce(
      &NearbyConnectionBroker::OnMojoDisconnection, base::Unretained(this)));
}

NearbyConnectionBroker::~NearbyConnectionBroker() = default;

void NearbyConnectionBroker::InvokeDisconnectedCallback() {
  message_sender_receiver_.reset();
  file_payload_handler_receiver_.reset();
  message_receiver_remote_.reset();
  std::move(on_disconnected_callback_).Run();
}

void NearbyConnectionBroker::NotifyConnected() {
  DCHECK(on_connected_callback_);
  std::move(on_connected_callback_).Run();
}

void NearbyConnectionBroker::NotifyMessageReceived(
    const std::string& received_message) {
  message_receiver_remote_->OnMessageReceived(received_message);
}

}  // namespace secure_channel
}  // namespace ash

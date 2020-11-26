// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_H_
#define CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chromeos/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace secure_channel {

// Attempts to create a Nearby Connection to a remote device and exchange
// messages on behalf of the SecureChannel service. Implements the
// mojom::NearbyMessageSender interface so that SecureChannel can send messages
// to Nearby Connections, and uses the mojom::NearbyMessageReceiver interface to
// relay messages received from Nearby Connections back to SecureChannel.
//
// An instance of this class is only meant to be used for one connection
// request to a single device. To make a new request, create a new object.
class NearbyConnectionBroker : public mojom::NearbyMessageSender {
 public:
  ~NearbyConnectionBroker() override;

 protected:
  NearbyConnectionBroker(
      const std::vector<uint8_t>& bluetooth_public_address,
      mojo::PendingReceiver<mojom::NearbyMessageSender> message_sender_receiver,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver_remote,
      base::OnceClosure on_connected_callback,
      base::OnceClosure on_disconnected_callback);

  const std::vector<uint8_t>& bluetooth_public_address() const {
    return bluetooth_public_address_;
  }

  // Can be overridden by derived classes to handle MessageSender and
  // MessageReceiver Mojo pipes being disconnected.
  virtual void OnMojoDisconnection() {}

  void InvokeDisconnectedCallback();
  void NotifyConnected();
  void NotifyMessageReceived(const std::string& received_message);

 private:
  std::vector<uint8_t> bluetooth_public_address_;
  mojo::Receiver<mojom::NearbyMessageSender> message_sender_receiver_;
  mojo::Remote<mojom::NearbyMessageReceiver> message_receiver_remote_;
  base::OnceClosure on_connected_callback_;
  base::OnceClosure on_disconnected_callback_;
};

}  // namespace secure_channel
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_NEARBY_CONNECTION_BROKER_H_

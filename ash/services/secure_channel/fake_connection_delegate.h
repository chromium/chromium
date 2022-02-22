// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_
#define ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_

#include "ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Test ConnectionDelegate implementation.
class FakeConnectionDelegate
    : public chromeos::secure_channel::mojom::ConnectionDelegate {
 public:
  FakeConnectionDelegate();

  FakeConnectionDelegate(const FakeConnectionDelegate&) = delete;
  FakeConnectionDelegate& operator=(const FakeConnectionDelegate&) = delete;

  ~FakeConnectionDelegate() override;

  mojo::PendingRemote<chromeos::secure_channel::mojom::ConnectionDelegate>
  GenerateRemote();
  void DisconnectGeneratedRemotes();

  const absl::optional<
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason>&
  connection_attempt_failure_reason() const {
    return connection_attempt_failure_reason_;
  }

  void set_closure_for_next_delegate_callback(base::OnceClosure closure) {
    closure_for_next_delegate_callback_ = std::move(closure);
  }

  const mojo::Remote<chromeos::secure_channel::mojom::Channel>& channel()
      const {
    return channel_;
  }

  const mojo::PendingReceiver<chromeos::secure_channel::mojom::MessageReceiver>&
  message_receiver_receiver() const {
    return message_receiver_receiver_;
  }

 private:
  // mojom::ConnectionDelegate:
  void OnConnectionAttemptFailure(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason reason)
      override;
  void OnConnection(
      mojo::PendingRemote<chromeos::secure_channel::mojom::Channel> channel,
      mojo::PendingReceiver<chromeos::secure_channel::mojom::MessageReceiver>
          message_receiver_receiver) override;

  void OnChannelDisconnected(uint32_t disconnection_reason,
                             const std::string& disconnection_description);

  mojo::ReceiverSet<chromeos::secure_channel::mojom::ConnectionDelegate>
      receivers_;
  base::OnceClosure closure_for_next_delegate_callback_;

  absl::optional<
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason>
      connection_attempt_failure_reason_;
  mojo::Remote<chromeos::secure_channel::mojom::Channel> channel_;
  mojo::PendingReceiver<chromeos::secure_channel::mojom::MessageReceiver>
      message_receiver_receiver_;
};

}  // namespace ash::secure_channel

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos::secure_channel {
using ::ash::secure_channel::FakeConnectionDelegate;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_

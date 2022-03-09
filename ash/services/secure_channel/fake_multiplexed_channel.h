// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FAKE_MULTIPLEXED_CHANNEL_H_
#define ASH_SERVICES_SECURE_CHANNEL_FAKE_MULTIPLEXED_CHANNEL_H_

#include <utility>
#include <vector>

#include "ash/services/secure_channel/client_connection_parameters.h"
#include "ash/services/secure_channel/connection_details.h"
#include "ash/services/secure_channel/multiplexed_channel.h"
#include "base/callback.h"

namespace ash::secure_channel {

// Test MultiplexedChannel implementation.
class FakeMultiplexedChannel : public MultiplexedChannel {
 public:
  FakeMultiplexedChannel(
      Delegate* delegate,
      ConnectionDetails connection_details,
      base::OnceCallback<void(const ConnectionDetails&)> destructor_callback =
          base::OnceCallback<void(const ConnectionDetails&)>());

  FakeMultiplexedChannel(const FakeMultiplexedChannel&) = delete;
  FakeMultiplexedChannel& operator=(const FakeMultiplexedChannel&) = delete;

  ~FakeMultiplexedChannel() override;

  std::vector<std::unique_ptr<ClientConnectionParameters>>& added_clients() {
    return added_clients_;
  }

  void SetDisconnecting();
  void SetDisconnected();

  // Make NotifyDisconnected() public for testing.
  using MultiplexedChannel::NotifyDisconnected;

 private:
  // MultiplexedChannel:
  bool IsDisconnecting() const override;
  bool IsDisconnected() const override;
  void PerformAddClientToChannel(std::unique_ptr<ClientConnectionParameters>
                                     client_connection_parameters) override;

  bool is_disconnecting_ = false;
  bool is_disconnected_ = false;

  std::vector<std::unique_ptr<ClientConnectionParameters>> added_clients_;

  base::OnceCallback<void(const ConnectionDetails&)> destructor_callback_;
};

// Test MultiplexedChannel::Delegate implementation.
class FakeMultiplexedChannelDelegate : public MultiplexedChannel::Delegate {
 public:
  FakeMultiplexedChannelDelegate();

  FakeMultiplexedChannelDelegate(const FakeMultiplexedChannelDelegate&) =
      delete;
  FakeMultiplexedChannelDelegate& operator=(
      const FakeMultiplexedChannelDelegate&) = delete;

  ~FakeMultiplexedChannelDelegate() override;

  const absl::optional<ConnectionDetails>& disconnected_connection_details() {
    return disconnected_connection_details_;
  }

 private:
  void OnDisconnected(const ConnectionDetails& connection_details) override;

  absl::optional<ConnectionDetails> disconnected_connection_details_;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_FAKE_MULTIPLEXED_CHANNEL_H_

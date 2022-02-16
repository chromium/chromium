// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_H_
#define ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_H_

#include <memory>

namespace chromeos {

namespace secure_channel {

class SecureChannel;

// Disconnects SecureChannel objects, which have an asynchronous
// disconnection flow. Deleting SecureChannel objects before they are
// fully disconnected can cause the underlying connection to remain open, which
// causes instability on the next connection attempt. See
// https://crbug.com/763604.
class SecureChannelDisconnector {
 public:
  SecureChannelDisconnector(const SecureChannelDisconnector&) = delete;
  SecureChannelDisconnector& operator=(const SecureChannelDisconnector&) =
      delete;

  virtual ~SecureChannelDisconnector() = default;

  virtual void DisconnectSecureChannel(
      std::unique_ptr<SecureChannel> channel_to_disconnect) = 0;

 protected:
  SecureChannelDisconnector() = default;
};

}  // namespace secure_channel

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::secure_channel {
using ::chromeos::secure_channel::SecureChannelDisconnector;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_H_

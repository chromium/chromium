// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_BASE_H_
#define ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_BASE_H_

#include "ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::secure_channel {

// Base SecureChannel implementation.
class SecureChannelBase : public mojom::SecureChannel {
 public:
  SecureChannelBase(const SecureChannelBase&) = delete;
  SecureChannelBase& operator=(const SecureChannelBase&) = delete;

  ~SecureChannelBase() override;

  // Binds a receiver to this implementation. Should be called each time that
  // the service receives a receiver.
  void BindReceiver(mojo::PendingReceiver<mojom::SecureChannel> receiver);

 protected:
  SecureChannelBase();

 private:
  mojo::ReceiverSet<mojom::SecureChannel> receivers_;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_BASE_H_

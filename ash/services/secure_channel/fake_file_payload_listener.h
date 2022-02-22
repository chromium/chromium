// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FAKE_FILE_PAYLOAD_LISTENER_H_
#define ASH_SERVICES_SECURE_CHANNEL_FAKE_FILE_PAYLOAD_LISTENER_H_

#include <vector>

#include "ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::secure_channel {

// Test FilePayloadListener implementation.
class FakeFilePayloadListener
    : public chromeos::secure_channel::mojom::FilePayloadListener {
 public:
  FakeFilePayloadListener();
  FakeFilePayloadListener(const FakeFilePayloadListener&) = delete;
  FakeFilePayloadListener& operator=(const FakeFilePayloadListener&) = delete;
  ~FakeFilePayloadListener() override;

  mojo::PendingRemote<chromeos::secure_channel::mojom::FilePayloadListener>
  GenerateRemote();

  void OnDisconnect();

  mojo::Receiver<chromeos::secure_channel::mojom::FilePayloadListener>&
  receiver() {
    return receiver_;
  }

  const std::vector<chromeos::secure_channel::mojom::FileTransferUpdatePtr>&
  received_updates() const {
    return received_updates_;
  }

  bool is_connected() const { return is_connected_; }

 private:
  // mojom::MessageReceiver:
  void OnFileTransferUpdate(
      chromeos::secure_channel::mojom::FileTransferUpdatePtr update) override;

  mojo::Receiver<chromeos::secure_channel::mojom::FilePayloadListener>
      receiver_{this};

  std::vector<chromeos::secure_channel::mojom::FileTransferUpdatePtr>
      received_updates_;
  bool is_connected_ = false;
};

}  // namespace ash::secure_channel

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos::secure_channel {
using ::ash::secure_channel::FakeFilePayloadListener;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_FAKE_FILE_PAYLOAD_LISTENER_H_

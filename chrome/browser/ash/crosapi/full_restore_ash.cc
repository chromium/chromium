// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/full_restore_ash.h"

namespace crosapi {

FullRestoreAsh::FullRestoreAsh() = default;

FullRestoreAsh::~FullRestoreAsh() = default;

void FullRestoreAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FullRestore> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void FullRestoreAsh::GetSessionInformation(
    GetSessionInformationCallback callback) {
  if (remotes_.empty()) {
    pending_callback_ = std::move(callback);
  } else {
    // TODO(sammiequon): Support multiple remotes.
    remotes_.begin()->get()->GetSessionInformation(std::move(callback));
  }
}

void FullRestoreAsh::AddFullRestoreClient(
    mojo::PendingRemote<mojom::FullRestoreClient> client) {
  remotes_.Add(mojo::Remote<mojom::FullRestoreClient>(std::move(client)));

  if (pending_callback_) {
    remotes_.begin()->get()->GetSessionInformation(
        std::move(pending_callback_));
  }
}

}  // namespace crosapi

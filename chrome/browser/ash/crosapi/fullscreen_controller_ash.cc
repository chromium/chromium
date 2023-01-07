// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/fullscreen_controller_ash.h"

#include "chromeos/crosapi/mojom/fullscreen_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

FullscreenControllerAsh::FullscreenControllerAsh() = default;
FullscreenControllerAsh::~FullscreenControllerAsh() = default;

void FullscreenControllerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FullscreenController> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void FullscreenControllerAsh::ShouldExitFullscreenBeforeLock(
    base::OnceCallback<void(bool)> callback) {
  // Ash should exit full screen before lock (which is the default) if there are
  // no remote clients.
  if (remotes_.empty()) {
    std::move(callback).Run(true);
    return;
  }

  // Assumes that there is only one remote client.
  remotes_.begin()->get()->ShouldExitFullscreenBeforeLock(
      base::BindOnce(&FullscreenControllerAsh::OnShouldExitFullscreenBeforeLock,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FullscreenControllerAsh::AddClient(
    mojo::PendingRemote<mojom::FullscreenControllerClient> client) {
  remotes_.Add(
      mojo::Remote<mojom::FullscreenControllerClient>(std::move(client)));
}

void FullscreenControllerAsh::OnShouldExitFullscreenBeforeLock(
    base::OnceCallback<void(bool)> callback,
    bool should_exit_fullscreen) {
  std::move(callback).Run(should_exit_fullscreen);
}

}  // namespace crosapi

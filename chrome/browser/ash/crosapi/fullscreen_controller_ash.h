// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FULLSCREEN_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FULLSCREEN_CONTROLLER_ASH_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/fullscreen_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the fullscreen controller crosapi interface.
class FullscreenControllerAsh : public mojom::FullscreenController {
 public:
  FullscreenControllerAsh();
  FullscreenControllerAsh(const FullscreenControllerAsh&) = delete;
  FullscreenControllerAsh& operator=(const FullscreenControllerAsh&) = delete;
  ~FullscreenControllerAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::FullscreenController> receiver);

  // Whether full screen mode should be exited on session lock/unlock.
  void ShouldExitFullscreenBeforeLock(base::OnceCallback<void(bool)> callback);

  // crosapi::mojom::FullscreenController:
  void AddClient(
      mojo::PendingRemote<mojom::FullscreenControllerClient> client) override;

 private:
  // Passed as a callback to `ShouldExitFullscreenBeforeLock` to the remote
  // client. Forwards the received response to Ash.
  void OnShouldExitFullscreenBeforeLock(base::OnceCallback<void(bool)> callback,
                                        bool should_exit_fullscreen);

  mojo::ReceiverSet<mojom::FullscreenController> receivers_;
  mojo::RemoteSet<mojom::FullscreenControllerClient> remotes_;

  base::WeakPtrFactory<FullscreenControllerAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FULLSCREEN_CONTROLLER_ASH_H_

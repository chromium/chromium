// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_EYE_DROPPER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_EYE_DROPPER_ASH_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/eye_dropper.mojom.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class EyeDropper;
}

namespace crosapi {

// Implements the crosapi EyeDropper interface. Lives in ash-chrome on the
// UI thread. Shows EyeDropper in response to mojo IPCs from lacros-chrome.
class EyeDropperAsh : public mojom::EyeDropper,
                      public content::EyeDropperListener {
 public:
  EyeDropperAsh();
  EyeDropperAsh(const EyeDropperAsh&) = delete;
  EyeDropperAsh& operator=(const EyeDropperAsh&) = delete;
  ~EyeDropperAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::EyeDropper> receiver);

  // crosapi::mojom::EyeDropper:
  void ShowEyeDropper(
      mojo::PendingRemote<mojom::EyeDropperListener> listener) override;

 private:
  // content::EyeDropperListener:
  void ColorSelected(SkColor color) override;
  void ColorSelectionCanceled() override;

  void OnDisconnect();

  std::unique_ptr<content::EyeDropper> eye_dropper_;
  mojo::Remote<mojom::EyeDropperListener> listener_;
  mojo::ReceiverSet<mojom::EyeDropper> receivers_;
  base::WeakPtrFactory<EyeDropperAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_EYE_DROPPER_ASH_H_

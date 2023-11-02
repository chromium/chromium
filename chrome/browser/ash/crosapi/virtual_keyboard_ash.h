// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VIRTUAL_KEYBOARD_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VIRTUAL_KEYBOARD_ASH_H_

#include "chromeos/crosapi/mojom/virtual_keyboard.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the VirtualKeyboard crosapi interface.
// This class must only be used from the main thread.
class VirtualKeyboardAsh : public mojom::VirtualKeyboard {
 public:
  VirtualKeyboardAsh();
  VirtualKeyboardAsh(const VirtualKeyboardAsh&) = delete;
  VirtualKeyboardAsh& operator=(const VirtualKeyboardAsh&) = delete;
  ~VirtualKeyboardAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::VirtualKeyboard> receiver);
  // mojom::VirtualKeyboard
  void RestrictFeatures(mojom::VirtualKeyboardRestrictionsPtr restrictions,
                        RestrictFeaturesCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::VirtualKeyboard> receivers_;

  base::WeakPtrFactory<VirtualKeyboardAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VIRTUAL_KEYBOARD_ASH_H_

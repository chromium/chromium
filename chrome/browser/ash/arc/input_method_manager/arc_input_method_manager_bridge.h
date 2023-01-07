// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/input_method_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace arc {

// The interface class encapsulates the detail of input method manager related
// IPC between Chrome and the ARC container.
class ArcInputMethodManagerBridge {
 public:
  virtual ~ArcInputMethodManagerBridge() = default;

  // Received mojo calls and connection state changes are passed to this
  // delegate.
  class Delegate : public mojom::InputMethodManagerHost {
   public:
    ~Delegate() override = default;

    // Mojo connection state changes:
    virtual void OnConnectionClosed() = 0;
  };

  // Sends mojo calls.
  using EnableImeCallback =
      mojom::InputMethodManagerInstance::EnableImeCallback;
  using SwitchImeToCallback =
      mojom::InputMethodManagerInstance::SwitchImeToCallback;

  virtual void SendEnableIme(const std::string& ime_id,
                             bool enable,
                             EnableImeCallback callback) = 0;
  virtual void SendSwitchImeTo(const std::string& ime_id,
                               SwitchImeToCallback callback) = 0;
  virtual void SendFocus(mojo::PendingRemote<mojom::InputConnection> connection,
                         mojom::TextInputStatePtr state) = 0;
  virtual void SendUpdateTextInputState(mojom::TextInputStatePtr state) = 0;
  virtual void SendShowVirtualKeyboard() = 0;
  virtual void SendHideVirtualKeyboard() = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_H_

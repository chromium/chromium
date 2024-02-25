// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_IMPL_H_

#include <string>
#include <vector>

#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_bridge.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace arc {

class ArcBridgeService;

class ArcInputMethodManagerBridgeImpl
    : public ArcInputMethodManagerBridge,
      public ConnectionObserver<mojom::InputMethodManagerInstance>,
      public mojom::InputMethodManagerHost {
 public:
  ArcInputMethodManagerBridgeImpl(Delegate* delegate,
                                  ArcBridgeService* bridge_service);

  ArcInputMethodManagerBridgeImpl(const ArcInputMethodManagerBridgeImpl&) =
      delete;
  ArcInputMethodManagerBridgeImpl& operator=(
      const ArcInputMethodManagerBridgeImpl&) = delete;

  ~ArcInputMethodManagerBridgeImpl() override;

  // ArcInputMethodManagerBridge overrides:
  void SendEnableIme(const std::string& ime_id,
                     bool enable,
                     EnableImeCallback callback) override;
  void SendSwitchImeTo(const std::string& ime_id,
                       SwitchImeToCallback callback) override;
  void SendFocus(mojo::PendingRemote<mojom::InputConnection> connection,
                 mojom::TextInputStatePtr state) override;
  void SendUpdateTextInputState(mojom::TextInputStatePtr state) override;
  void SendShowVirtualKeyboard() override;
  void SendHideVirtualKeyboard() override;

  // ConnectionObserver<mojom::InputMethodManagerInstance> overrides:
  void OnConnectionClosed() override;

  // mojom::InputMethodManagerHost overrides:
  void OnActiveImeChanged(const std::string& ime_id) override;
  void OnImeDisabled(const std::string& ime_id) override;
  void OnImeInfoChanged(std::vector<mojom::ImeInfoPtr> ime_info_array) override;

 private:
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<ArcBridgeService>
      bridge_service_;  // Owned by ArcServiceManager
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_IMPL_H_

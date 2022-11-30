// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_TEST_INPUT_METHOD_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_TEST_INPUT_METHOD_MANAGER_BRIDGE_H_

#include <string>
#include <tuple>
#include <vector>

#include "ash/components/arc/mojom/input_method_manager.mojom-forward.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_bridge.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace arc {

class TestInputMethodManagerBridge : public ArcInputMethodManagerBridge {
 public:
  TestInputMethodManagerBridge();

  TestInputMethodManagerBridge(const TestInputMethodManagerBridge&) = delete;
  TestInputMethodManagerBridge& operator=(const TestInputMethodManagerBridge&) =
      delete;

  ~TestInputMethodManagerBridge() override;

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

  std::vector<std::tuple<std::string, bool>> enable_ime_calls_;
  std::vector<std::string> switch_ime_to_calls_;
  int focus_calls_count_ = 0;
  int update_text_input_state_calls_count_ = 0;
  mojom::TextInputStatePtr last_text_input_state_ = nullptr;
  int show_virtual_keyboard_calls_count_ = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_TEST_INPUT_METHOD_MANAGER_BRIDGE_H_

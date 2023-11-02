// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/test_input_method_manager_bridge.h"

namespace arc {

TestInputMethodManagerBridge::TestInputMethodManagerBridge() = default;
TestInputMethodManagerBridge::~TestInputMethodManagerBridge() = default;

void TestInputMethodManagerBridge::SendEnableIme(const std::string& ime_id,
                                                 bool enable,
                                                 EnableImeCallback callback) {
  enable_ime_calls_.push_back(std::make_tuple(ime_id, enable));
  std::move(callback).Run(true);
}

void TestInputMethodManagerBridge::SendSwitchImeTo(
    const std::string& ime_id,
    SwitchImeToCallback callback) {
  switch_ime_to_calls_.push_back(ime_id);
  std::move(callback).Run(true);
}

void TestInputMethodManagerBridge::SendFocus(
    mojo::PendingRemote<mojom::InputConnection> connection,
    mojom::TextInputStatePtr state) {
  ++focus_calls_count_;
}

void TestInputMethodManagerBridge::SendUpdateTextInputState(
    mojom::TextInputStatePtr state) {
  ++update_text_input_state_calls_count_;
  last_text_input_state_ = state.Clone();
}

void TestInputMethodManagerBridge::SendShowVirtualKeyboard() {
  ++show_virtual_keyboard_calls_count_;
}

void TestInputMethodManagerBridge::SendHideVirtualKeyboard() {}

}  // namespace arc

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/test_ime_controller_client.h"

#include <memory>
#include <string>
#include <utility>

namespace ash {

TestImeControllerClient::TestImeControllerClient() = default;

TestImeControllerClient::~TestImeControllerClient() = default;

void TestImeControllerClient::SwitchToNextIme() {
  ++next_ime_count_;
}

void TestImeControllerClient::SwitchToLastUsedIme() {
  ++last_used_ime_count_;
}

void TestImeControllerClient::SwitchImeById(const std::string& id,
                                            bool show_message) {
  ++switch_ime_count_;
  last_switch_ime_id_ = id;
  last_show_message_ = show_message;
}

void TestImeControllerClient::ActivateImeMenuItem(const std::string& key) {}

void TestImeControllerClient::SetCapsLockEnabled(bool enabled) {
  ++set_caps_lock_count_;
}

void TestImeControllerClient::OverrideKeyboardKeyset(
    input_method::ImeKeyset keyset,
    OverrideKeyboardKeysetCallback callback) {
  last_keyset_ = keyset;
  std::move(callback).Run();
}

void TestImeControllerClient::ShowModeIndicator() {
  ++show_mode_indicator_count_;
}

}  // namespace ash

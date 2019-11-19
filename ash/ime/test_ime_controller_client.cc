// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/test_ime_controller_client.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/mojom/ime_controller.mojom.h"

namespace ash {

TestImeControllerClient::TestImeControllerClient() = default;

TestImeControllerClient::~TestImeControllerClient() = default;

mojo::PendingRemote<mojom::ImeControllerClient>
TestImeControllerClient::CreateRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

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
    chromeos::input_method::mojom::ImeKeyset keyset,
    OverrideKeyboardKeysetCallback callback) {
  last_keyset_ = keyset;
  std::move(callback).Run();
}

void TestImeControllerClient::UpdateMirroringState(bool enabled) {
  is_mirroring_ = enabled;
}

void TestImeControllerClient::UpdateCastingState(bool enabled) {
  is_casting_ = enabled;
}

void TestImeControllerClient::ShowModeIndicator() {
  ++show_mode_indicator_count_;
}

}  // namespace ash

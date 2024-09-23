// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/test_ime_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ime_info.h"

ImeControllerResetterForTest::ImeControllerResetterForTest()
    : instance_(ash::ImeController::Get()) {
  ash::ImeController::SetInstanceForTest(nullptr);
}

ImeControllerResetterForTest::~ImeControllerResetterForTest() {
  ash::ImeController::SetInstanceForTest(instance_);
}

TestImeController::TestImeController() = default;

TestImeController::~TestImeController() = default;

void TestImeController::AddObserver(Observer* observer) {}
void TestImeController::RemoveObserver(Observer* observer) {}

void TestImeController::SetClient(ash::ImeControllerClient* client) {}

void TestImeController::RefreshIme(const std::string& current_ime_id,
                                   std::vector<ash::ImeInfo> available_imes,
                                   std::vector<ash::ImeMenuItem> menu_items) {
  current_ime_id_ = current_ime_id;
  available_imes_ = std::move(available_imes);
  menu_items_ = std::move(menu_items);
}

void TestImeController::SetImesManagedByPolicy(bool managed) {
  managed_by_policy_ = managed;
}

void TestImeController::ShowImeMenuOnShelf(bool show) {
  show_ime_menu_on_shelf_ = show;
}

void TestImeController::UpdateCapsLockState(bool enabled) {
  is_caps_lock_enabled_ = enabled;
}

void TestImeController::OnKeyboardLayoutNameChanged(
    const std::string& layout_name) {
  keyboard_layout_name_ = layout_name;
}

void TestImeController::SetExtraInputOptionsEnabledState(
    bool is_extra_input_options_enabled,
    bool is_emoji_enabled,
    bool is_handwriting_enabled,
    bool is_voice_enabled) {
  is_extra_input_options_enabled_ = is_extra_input_options_enabled;
  is_emoji_enabled_ = is_emoji_enabled;
  is_handwriting_enabled_ = is_handwriting_enabled;
  is_voice_enabled_ = is_voice_enabled;
}

void TestImeController::ShowModeIndicator(const gfx::Rect& anchor_bounds,
                                          const std::u16string& text) {
  show_mode_indicator_ = true;
}

bool TestImeController::IsCapsLockEnabled() const {
  return is_caps_lock_enabled_;
}

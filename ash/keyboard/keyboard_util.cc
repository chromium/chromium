// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/keyboard_util.h"

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/wm/window_util.h"

namespace ash {

namespace keyboard_util {

bool IsArrowKeyCode(const ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_DOWN || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP;
}

bool CloseKeyboardIfActive() {
  // Close the Chrome VK if it is visible.
  auto* keyboard_controller = KeyboardController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboard(HideReason::kUser);
    return true;
  }

  // Close the Android VK if it is visible by sending a back event.
  if (Shell::Get()->system_tray_model()->virtual_keyboard()->visible()) {
    window_util::SendBackKeyEvent(Shell::GetPrimaryRootWindow());
    return true;
  }

  return false;
}

}  // namespace keyboard_util

}  // namespace ash

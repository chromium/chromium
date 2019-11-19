// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "base/command_line.h"
#include "base/unguessable_token.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/size.h"

namespace keyboard {

KeyboardUI::KeyboardUI() = default;

KeyboardUI::~KeyboardUI() = default;

void KeyboardUI::ShowKeyboardWindow() {
  DVLOG(1) << "ShowKeyboardWindow";
  aura::Window* window = GetKeyboardWindow();
  if (window) {
    TRACE_EVENT0("vk", "ShowKeyboardWindow");
    window->Show();
  }
}

void KeyboardUI::HideKeyboardWindow() {
  DVLOG(1) << "HideKeyboardWindow";
  aura::Window* window = GetKeyboardWindow();
  if (window)
    window->Hide();
}

void KeyboardUI::SetController(KeyboardUIController* controller) {
  keyboard_controller_ = controller;
}

}  // namespace keyboard

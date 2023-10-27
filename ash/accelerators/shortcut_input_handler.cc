// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/shortcut_input_handler.h"

#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

constexpr int kKeyboardModifierFlags = ui::EF_CONTROL_DOWN |
                                       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN |
                                       ui::EF_ALT_DOWN;

}  // namespace

ShortcutInputHandler::ShortcutInputHandler() = default;
ShortcutInputHandler::~ShortcutInputHandler() = default;

void ShortcutInputHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (event->is_repeat() || observers_.empty()) {
    return;
  }

  mojom::KeyEvent key_event(
      event->key_code(), static_cast<int>(event->code()),
      static_cast<int>(event->GetDomKey()),
      event->flags() & kKeyboardModifierFlags,
      base::UTF16ToUTF8(GetKeyDisplay(event->key_code())));
  if (event->type() == ui::ET_KEY_PRESSED) {
    for (auto& observer : observers_) {
      observer.OnShortcutInputEventPressed(key_event);
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnShortcutInputEventReleased(key_event);
    }
  }
}

void ShortcutInputHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShortcutInputHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash

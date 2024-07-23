// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/shortcut_input_handler.h"

#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

constexpr int kKeyboardModifierFlags = ui::EF_CONTROL_DOWN |
                                       ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN |
                                       ui::EF_ALT_DOWN | ui::EF_FUNCTION_DOWN;

ui::KeyboardCode RetrieveKeyCode(const ui::KeyEvent& event) {
  // Remap positional keys in the current layout to the corresponding US layout
  // KeyboardCode.
  ui::KeyboardCode key_code =
      ui::KeycodeConverter::MapPositionalDomCodeToUSShortcutKey(
          event.code(), event.key_code());
  if (key_code == ui::VKEY_UNKNOWN) {
    key_code = event.key_code();
  }
  // TODO(b/327436148): Fix display mirror icon
  if (key_code == ui::VKEY_UNKNOWN &&
      event.code() == ui::DomCode::SHOW_ALL_WINDOWS) {
    // Show all windows is through VKEY_MEDIA_LAUNCH_APP1.
    key_code = ui::VKEY_MEDIA_LAUNCH_APP1;
  }

  if (ui::HasRightAltProperty(event)) {
    key_code = ui::VKEY_RIGHT_ALT;
  }

  return key_code;
}

}  // namespace

ShortcutInputHandler::ShortcutInputHandler() = default;

ShortcutInputHandler::~ShortcutInputHandler() {
  CHECK(Shell::Get());
  auto* event_forwarder =
      Shell::Get()->event_rewriter_controller()->prerewritten_event_forwarder();
  if (initialized_ && event_forwarder) {
    event_forwarder->RemoveObserver(this);
  }
}

void ShortcutInputHandler::Initialize() {
  CHECK(Shell::Get());
  if (!features::IsPeripheralCustomizationEnabled() &&
      !::features::IsShortcutCustomizationEnabled()) {
    LOG(ERROR) << "ShortcutInputHandler can only be initialized if "
               << "shortcut or peripherals customization flags are enabled.";
    return;
  }

  auto* event_forwarder =
      Shell::Get()->event_rewriter_controller()->prerewritten_event_forwarder();
  if (!event_forwarder) {
    LOG(ERROR) << "Attempted to initialiaze ShortcutInputHandler before "
               << "PrerewrittenEventForwarder was initialized.";
    return;
  }

  initialized_ = true;
  event_forwarder->AddObserver(this);
}

void ShortcutInputHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (event->is_repeat() || observers_.empty()) {
    return;
  }

  const ui::KeyboardCode key_code = RetrieveKeyCode(*event);
  mojom::KeyEvent key_event(key_code, static_cast<int>(event->code()),
                            static_cast<int>(event->GetDomKey()),
                            event->flags() & kKeyboardModifierFlags,
                            base::UTF16ToUTF8(GetKeyDisplay(key_code)));
  if (event->type() == ui::EventType::kKeyPressed) {
    for (auto& observer : observers_) {
      observer.OnShortcutInputEventPressed(key_event);
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnShortcutInputEventReleased(key_event);
    }
  }

  if (should_consume_key_events_) {
    event->StopPropagation();
  }
}

void ShortcutInputHandler::OnPrerewriteKeyInputEvent(
    const ui::KeyEvent& event) {
  if (event.is_repeat() || observers_.empty()) {
    return;
  }

  const ui::KeyboardCode key_code = RetrieveKeyCode(event);
  mojom::KeyEvent key_event(key_code, static_cast<int>(event.code()),
                            static_cast<int>(event.GetDomKey()),
                            event.flags() & kKeyboardModifierFlags,
                            base::UTF16ToUTF8(GetKeyDisplay(key_code)));
  if (event.type() == ui::EventType::kKeyPressed) {
    for (auto& observer : observers_) {
      observer.OnPrerewrittenShortcutInputEventPressed(key_event);
    }
  } else {
    for (auto& observer : observers_) {
      observer.OnPrerewrittenShortcutInputEventReleased(key_event);
    }
  }
}

void ShortcutInputHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShortcutInputHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ShortcutInputHandler::SetShouldConsumeKeyEvents(
    bool should_consume_key_events) {
  should_consume_key_events_ = should_consume_key_events;
}

}  // namespace ash

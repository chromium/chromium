// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager_chromeos.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "ui/aura/env.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/native_cursor_manager.h"

namespace ash {

CursorManager::CursorManager(std::unique_ptr<wm::NativeCursorManager> delegate)
    : wm::CursorManager(std::move(delegate)) {}

CursorManager::~CursorManager() = default;

void CursorManager::Init() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceShowCursor)) {
    // Set a custom cursor so users know that the switch is turned on.
    gfx::NativeCursor cursor(ui::mojom::CursorType::kCustom);
    const gfx::ImageSkia custom_icon =
        gfx::CreateVectorIcon(kTouchIndicatorIcon, SK_ColorBLACK);
    SkBitmap bitmap = *custom_icon.bitmap();
    gfx::Point hotspot(bitmap.width() / 2, bitmap.height() / 2);
    auto* cursor_factory = ui::CursorFactory::GetInstance();
    cursor.SetPlatformCursor(
        cursor_factory->CreateImageCursor(cursor.type(), bitmap, hotspot));
    cursor.set_custom_bitmap(bitmap);
    cursor.set_custom_hotspot(hotspot);

    SetCursor(cursor);
    LockCursor();
    return;
  }

  // Hide the mouse cursor on startup.
  HideCursor();
  SetCursor(ui::mojom::CursorType::kPointer);
}

bool CursorManager::ShouldHideCursorOnKeyEvent(
    const ui::KeyEvent& event) const {
  if (event.type() != ui::ET_KEY_PRESSED)
    return false;

  // Pressing one key repeatedly will not hide the cursor.
  // To deal with the issue 855163 (http://crbug.com/855163).
  if (event.is_repeat())
    return false;

  // Do not hide cursor when clicking the key with mouse button pressed.
  if (aura::Env::GetInstance()->IsMouseButtonDown())
    return false;

  // Clicking on a key when the accessibility virtual keyboard is enabled should
  // not hide the cursor.
  if (keyboard::GetAccessibilityKeyboardEnabled())
    return false;

  // Clicking on a key in the virtual keyboard should not hide the cursor.
  if (keyboard::KeyboardUIController::HasInstance() &&
      keyboard::KeyboardUIController::Get()->IsKeyboardVisible()) {
    return false;
  }

  // All alt, control and command key commands are ignored.
  if (event.IsAltDown() || event.IsControlDown() || event.IsCommandDown())
    return false;

  ui::KeyboardCode code = event.key_code();
  if (code >= ui::VKEY_F1 && code <= ui::VKEY_F24)
    return false;
  if (code >= ui::VKEY_BROWSER_BACK && code <= ui::VKEY_MEDIA_LAUNCH_APP2)
    return false;
  switch (code) {
    // Modifiers.
    case ui::VKEY_SHIFT:
    case ui::VKEY_CONTROL:
    case ui::VKEY_MENU:
    // Search key == VKEY_LWIN.
    case ui::VKEY_LWIN:
    case ui::VKEY_WLAN:
    case ui::VKEY_POWER:
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_DOWN:
    case ui::VKEY_PRIVACY_SCREEN_TOGGLE:
    case ui::VKEY_ZOOM:
      return false;
    default:
      return true;
  }
}

}  // namespace ash

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/user_input_impl.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/public/mojom/user_input.mojom.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/mojom/event_mojom_traits.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

aura::WindowTreeHost* GetHostForPrimaryDisplay() {
  display::Screen* screen = display::Screen::GetScreen();
  CHECK(screen);

  aura::WindowTreeHost* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  CHECK(host);
  return host;
}

}  // namespace

UserInputImpl::UserInputImpl() = default;
UserInputImpl::~UserInputImpl() = default;

void UserInputImpl::Bind(
    mojo::PendingReceiver<ax::mojom::UserInput> ui_receiver) {
  ui_receivers_.Add(this, std::move(ui_receiver));
}

// TODO(b/311415118): Convert to actions in the service process, instead of
// sending full key events.
void UserInputImpl::SendSyntheticKeyEventForShortcutOrNavigation(
    ax::mojom::SyntheticKeyEventPtr key_event) {
  // TODO(b/307553499): Convert SyntheticKeyEvent to use dom_code and dom_key.
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(key_event->key_data->key_code);
  ui::EventType type = mojo::ConvertTo<ui::EventType>(key_event->type);
  ui::KeyEvent synthetic_key_event(type, key_code, key_event->flags);

  auto* host = GetHostForPrimaryDisplay();
  // Skips sending to rewriters.
  host->DeliverEventToSink(&synthetic_key_event);
}

void UserInputImpl::SendSyntheticMouseEvent(
    ax::mojom::SyntheticMouseEventPtr mouse_event) {
  ui::EventType type = mojo::ConvertTo<ui::EventType>(mouse_event->type);

  int flags = 0;
  if (type != ui::EventType::kMouseMoved) {
    if (mouse_event->mouse_button) {
      switch (*mouse_event->mouse_button) {
        case ax::mojom::SyntheticMouseEventButton::kLeft:
          flags |= ui::EF_LEFT_MOUSE_BUTTON;
          break;
        case ax::mojom::SyntheticMouseEventButton::kMiddle:
          flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
          break;
        case ax::mojom::SyntheticMouseEventButton::kRight:
          flags |= ui::EF_RIGHT_MOUSE_BUTTON;
          break;
        case ax::mojom::SyntheticMouseEventButton::kBack:
          flags |= ui::EF_BACK_MOUSE_BUTTON;
          break;
        case ax::mojom::SyntheticMouseEventButton::kForward:
          flags |= ui::EF_FORWARD_MOUSE_BUTTON;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    } else {
      // If no mouse button is provided, use kLeft.
      flags |= ui::EF_LEFT_MOUSE_BUTTON;
    }
  }

  int changed_button_flags = flags;

  flags |= ui::EF_IS_SYNTHESIZED;
  if (mouse_event->touch_accessibility && *(mouse_event->touch_accessibility)) {
    flags |= ui::EF_TOUCH_ACCESSIBILITY;
  }

  AccessibilityManager::Get()->SendSyntheticMouseEvent(
      type, flags, changed_button_flags, mouse_event->point);
}

}  // namespace ash

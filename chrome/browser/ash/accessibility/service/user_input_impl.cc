// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/user_input_impl.h"

#include "ash/public/cpp/window_tree_host_lookup.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/public/mojom/user_input.mojom.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
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

}  // namespace ash

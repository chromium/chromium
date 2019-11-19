// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/keyboard_driven_event_rewriter.h"

#include "ash/keyboard/keyboard_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace ash {

KeyboardDrivenEventRewriter::KeyboardDrivenEventRewriter() = default;

KeyboardDrivenEventRewriter::~KeyboardDrivenEventRewriter() = default;

ui::EventRewriteStatus KeyboardDrivenEventRewriter::RewriteForTesting(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  return Rewrite(event, rewritten_event);
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (!enabled_ ||
      Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  return Rewrite(event, rewritten_event);
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::Rewrite(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  int flags = event.flags();
  const int kModifierMask = ui::EF_SHIFT_DOWN;
  if ((flags & kModifierMask) != kModifierMask)
    return ui::EVENT_REWRITE_CONTINUE;

  DCHECK(event.type() == ui::ET_KEY_PRESSED ||
         event.type() == ui::ET_KEY_RELEASED)
      << "Unexpected event type " << event.type();
  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
  ui::KeyboardCode key_code = key_event.key_code();

  if (!ash::keyboard_util::IsArrowKeyCode(key_code) &&
      key_code != ui::VKEY_RETURN && key_code != ui::VKEY_F6) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  ui::EventRewriterChromeOS::MutableKeyState state = {
      flags & ~(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
      key_event.code(), key_event.GetDomKey(), key_event.key_code()};

  if (arrow_to_tab_rewriting_enabled_) {
    if (ash::keyboard_util::IsArrowKeyCode(key_code)) {
      const ui::KeyEvent tab_event(ui::ET_KEY_PRESSED, ui::VKEY_TAB,
                                   ui::EF_NONE);
      state.code = tab_event.code();
      state.key = tab_event.GetDomKey();
      state.key_code = tab_event.key_code();
      if (key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP)
        state.flags |= ui::EF_SHIFT_DOWN;
    }
  }

  ui::EventRewriterChromeOS::BuildRewrittenKeyEvent(key_event, state,
                                                    rewritten_event);
  return ui::EVENT_REWRITE_REWRITTEN;
}

}  // namespace ash

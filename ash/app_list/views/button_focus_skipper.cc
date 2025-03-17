// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/button_focus_skipper.h"

#include "base/check.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/views/view.h"

namespace ash {

ButtonFocusSkipper::ButtonFocusSkipper(ui::EventTarget* event_target)
    : event_target_(event_target) {
  event_target_->AddPreTargetHandler(this);
}

ButtonFocusSkipper::~ButtonFocusSkipper() {
  event_target_->RemovePreTargetHandler(this);
}

void ButtonFocusSkipper::AddButton(views::View* button) {
  DCHECK(button);
  buttons_.push_back(button);
}

void ButtonFocusSkipper::OnEvent(ui::Event* event) {
  // Don't adjust focus behavior if the user already focused the button.
  for (views::View* button : buttons_) {
    if (button->HasFocus()) {
      return;
    }
  }

  bool skip_focus = false;
  // This class overrides OnEvent() to examine all events so that focus
  // behavior is restored by mouse events, gesture events, etc.
  if (event->type() == ui::EventType::kKeyPressed) {
    ui::KeyboardCode key = event->AsKeyEvent()->key_code();
    if (key == ui::VKEY_UP || key == ui::VKEY_DOWN) {
      skip_focus = true;
    }
  }
  for (views::View* button : buttons_) {
    button->SetFocusBehavior(skip_focus ? views::View::FocusBehavior::NEVER
                                        : views::View::FocusBehavior::ALWAYS);
  }
}

}  // namespace ash

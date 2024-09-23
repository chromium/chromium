// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_textfield_controller.h"

#include "ui/events/types/event_type.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {
SystemTextfieldController::SystemTextfieldController(SystemTextfield* textfield)
    : textfield_(textfield) {
  textfield_->SetController(this);
}

SystemTextfieldController::~SystemTextfieldController() {
  textfield_->SetController(nullptr);
}

bool SystemTextfieldController::HandleKeyEvent(views::Textfield* sender,
                                               const ui::KeyEvent& key_event) {
  DCHECK_EQ(textfield_, sender);

  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  const bool active = textfield_->IsActive();
  if (key_event.key_code() == ui::VKEY_RETURN) {
    // If the textfield is focused but not active, activate the textfield and
    // highlight all the text.
    if (!active) {
      textfield_->SetActive(true);
      textfield_->SelectAll(false);
      return true;
    }

    // Otherwise, commit the changes and deactivate the textfield.
    textfield_->SetActive(false);
    return true;
  }

  if (key_event.key_code() == ui::VKEY_ESCAPE) {
    // If the textfield is active, discard the changes, deactivate the
    // textfield.
    if (active) {
      textfield_->RestoreText();
      // `RestoreText()`, uses `SetText()`, which does not invoke
      // `ContentsChanged()`. Call `ContentsChanged()` directly, so the text
      // change gets handled by controller overrides.
      ContentsChanged(textfield_, textfield_->GetText());
      textfield_->SetActive(false);
      return true;
    }
  }

  return false;
}

bool SystemTextfieldController::HandleMouseEvent(
    views::Textfield* sender,
    const ui::MouseEvent& mouse_event) {
  DCHECK_EQ(textfield_, sender);

  if (!mouse_event.IsOnlyLeftMouseButton() &&
      !mouse_event.IsOnlyRightMouseButton()) {
    return false;
  }

  switch (mouse_event.type()) {
    case ui::EventType::kMousePressed:
      // When the mouse is pressed and the textfield is not active, activate
      // the textfield but defer selecting all text until the mouse is
      // released.
      if (!textfield_->IsActive()) {
        defer_select_all_ = true;
        textfield_->SetActive(true);
      }
      break;
    case ui::EventType::kMouseReleased:
      // When selecting all text was deferred, do it if there is no selection.
      if (defer_select_all_) {
        defer_select_all_ = false;
        if (!textfield_->HasSelection()) {
          textfield_->SelectAll(false);
        }
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

bool SystemTextfieldController::HandleGestureEvent(
    views::Textfield* sender,
    const ui::GestureEvent& gesture_event) {
  DCHECK_EQ(sender, textfield_);

  // Activate the textfield when receiving gesture event.
  if (!textfield_->IsActive()) {
    textfield_->SetActive(true);
  }

  // Select all text after tapping once.
  if (gesture_event.type() == ui::EventType::kGestureTap &&
      gesture_event.details().tap_count() == 1 && !textfield_->HasSelection()) {
    textfield_->SelectAll(false);
  }

  return false;
}

}  // namespace ash

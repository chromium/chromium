// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_textfield_controller.h"

#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {
SystemTextfieldController::SystemTextfieldController(SystemTextfield* textfield)
    : textfield_(textfield) {
  textfield_->SetController(this);
  textfield_->set_delegate(this);
  views::FocusRing::Get(textfield_)
      ->SetHasFocusPredicate(
          [&](views::View* view) { return textfield_->active(); });
}

SystemTextfieldController::~SystemTextfieldController() = default;

void SystemTextfieldController::OnTextfieldFocused(SystemTextfield* textfield) {
  DCHECK_EQ(textfield_, textfield);
  // Do not activate the textfield immediately on focus.
}

void SystemTextfieldController::OnTextfieldBlurred(SystemTextfield* textfield) {
  DCHECK_EQ(textfield_, textfield);
  // Deactivate the textfield on blur.
  textfield_->SetActive(false);
}

bool SystemTextfieldController::HandleKeyEvent(views::Textfield* sender,
                                               const ui::KeyEvent& key_event) {
  DCHECK_EQ(textfield_, sender);

  if (key_event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }

  const bool active = textfield_->active();
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
    case ui::ET_MOUSE_PRESSED:
      // When the mouse is pressed and the textfield is not active, activate
      // the textfield but defer selecting all text until the mouse is
      // released.
      if (!textfield_->active()) {
        defer_select_all_ = true;
        textfield_->SetActive(true);
      }
      break;
    case ui::ET_MOUSE_RELEASED:
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

}  // namespace ash
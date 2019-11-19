// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_util.h"

#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ui/aura/window.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

bool IsUnhandledUnmodifiedEvent(const ui::KeyEvent& event) {
  if (event.handled() || event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (event.IsShiftDown() || event.IsControlDown() || event.IsAltDown())
    return false;

  return true;
}

bool IsUnhandledLeftRightKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return event.key_code() == ui::VKEY_LEFT ||
         event.key_code() == ui::VKEY_RIGHT;
}

bool IsUnhandledUpDownKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN;
}

bool IsUnhandledArrowKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return IsArrowKeyEvent(event);
}

bool IsArrowKeyEvent(const ui::KeyEvent& event) {
  return IsArrowKey(event.key_code());
}

bool IsArrowKey(const ui::KeyboardCode& key_code) {
  return key_code == ui::VKEY_DOWN || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP;
}

bool LeftRightKeyEventShouldExitText(views::Textfield* textfield,
                                     const ui::KeyEvent& key_event) {
  DCHECK(IsUnhandledLeftRightKeyEvent(key_event));

  if (textfield->GetText().empty())
    return true;

  if (textfield->HasSelection())
    return false;

  if (textfield->GetCursorPosition() != 0 &&
      textfield->GetCursorPosition() != textfield->GetText().length()) {
    return false;
  }

  // For RTL language, the beginning position of the cursor will be at the right
  // side and it grows towards left as we are typing.
  const bool text_rtl =
      textfield->GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
  const bool cursor_at_beginning = textfield->GetCursorPosition() == 0;
  const bool move_cursor_reverse =
      (text_rtl && key_event.key_code() == ui::VKEY_RIGHT) ||
      (!text_rtl && key_event.key_code() == ui::VKEY_LEFT);

  if ((cursor_at_beginning && !move_cursor_reverse) ||
      (!cursor_at_beginning && move_cursor_reverse)) {
    // Cursor is at either the beginning or the end of the textfield, and it
    // will move inward.
    return false;
  }

  return true;
}

bool ProcessLeftRightKeyTraversalForTextfield(views::Textfield* textfield,
                                              const ui::KeyEvent& key_event) {
  DCHECK(IsUnhandledLeftRightKeyEvent(key_event));

  if (!LeftRightKeyEventShouldExitText(textfield, key_event))
    return false;

  const bool move_focus_reverse = base::i18n::IsRTL()
                                      ? key_event.key_code() == ui::VKEY_RIGHT
                                      : key_event.key_code() == ui::VKEY_LEFT;

  // Move focus outside the textfield.
  textfield->GetFocusManager()->AdvanceFocus(move_focus_reverse);
  return true;
}

void UpdateActivationForAppListView(AppListView* app_list_view,
                                    bool is_tablet_mode) {
  views::Widget* widget = app_list_view->GetWidget();
  const aura::Window* active_window =
      wm::GetActivationClient(widget->GetNativeWindow()->GetRootWindow())
          ->GetActiveWindow();

  // After switching to tablet mode, other app windows may be active. Show the
  // app list without activating it to avoid breaking other windows' state.
  if (is_tablet_mode && active_window)
    return;

  widget->Show();

  // Refocus the embedded assistant page after widget activation.
  auto* contents_view = app_list_view->app_list_main_view()->contents_view();
  if (contents_view->IsShowingEmbeddedAssistantUI())
    contents_view->FocusEmbeddedAssistantPage();
}

}  // namespace ash

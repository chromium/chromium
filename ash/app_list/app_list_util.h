// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_UTIL_H_
#define ASH_APP_LIST_APP_LIST_UTIL_H_

#include "ash/app_list/app_list_export.h"
#include "ui/events/event.h"

namespace views {
class Textfield;
}

namespace ash {
class AppListView;

// Returns true if the key event is an unhandled left or right arrow (unmodified
// by ctrl, shift, or alt)
APP_LIST_EXPORT bool IsUnhandledLeftRightKeyEvent(const ui::KeyEvent& event);

// Returns true if the key event is an unhandled up or down arrow (unmodified by
// ctrl, shift, or alt)
APP_LIST_EXPORT bool IsUnhandledUpDownKeyEvent(const ui::KeyEvent& event);

// Returns true if the key event is an unhandled arrow key event of any type
// (unmodified by ctrl, shift, or alt)
APP_LIST_EXPORT bool IsUnhandledArrowKeyEvent(const ui::KeyEvent& event);

// Returns whether the event is an arrow key event.
APP_LIST_EXPORT bool IsArrowKeyEvent(const ui::KeyEvent& event);

// Returns true if the keyboard code is one of: |VKEY_UP|, |VKEY_LEFT|,
// |VKEY_RIGHT|, |VKEY_DOWN|
APP_LIST_EXPORT bool IsArrowKey(const ui::KeyboardCode& key_code);

// Returns true if the arrow key event should move focus away from the
// |textfield|. This is usually when the insertion point would move away from
// text.
APP_LIST_EXPORT bool LeftRightKeyEventShouldExitText(
    views::Textfield* textfield,
    const ui::KeyEvent& key_event);

// Processes left/right key traversal for the given |textfield|. Returns true
// if focus is moved.
APP_LIST_EXPORT bool ProcessLeftRightKeyTraversalForTextfield(
    views::Textfield* textfield,
    const ui::KeyEvent& key_event);

// Updates the activation for |app_list_view|. Intended to be a callback
// function for when the view's bounds are finished animating.
APP_LIST_EXPORT void UpdateActivationForAppListView(AppListView* app_list_view,
                                                    bool is_tablet_mode);

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_UTIL_H_

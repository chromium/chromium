// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_UTIL_H_
#define ASH_APP_LIST_APP_LIST_UTIL_H_

#include "ash/ash_export.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"

namespace views {
class Textfield;
}  // namespace views

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {
class AppListItem;

// Returns true if the key event is an unhandled left or right arrow (unmodified
// by ctrl, shift, or alt)
ASH_EXPORT bool IsUnhandledLeftRightKeyEvent(const ui::KeyEvent& event);

// Returns true if the key event is an unhandled up or down arrow (unmodified by
// ctrl, shift, or alt)
ASH_EXPORT bool IsUnhandledUpDownKeyEvent(const ui::KeyEvent& event);

// Returns true if the key event is an unhandled arrow key event of any type
// (unmodified by ctrl, shift, or alt)
ASH_EXPORT bool IsUnhandledArrowKeyEvent(const ui::KeyEvent& event);

// Returns whether the event is an arrow key event.
ASH_EXPORT bool IsArrowKeyEvent(const ui::KeyEvent& event);

// Returns true if the keyboard code is one of: |VKEY_UP|, |VKEY_LEFT|,
// |VKEY_RIGHT|, |VKEY_DOWN|
ASH_EXPORT bool IsArrowKey(const ui::KeyboardCode& key_code);

// Returns true if the |item| is a folder item.
ASH_EXPORT bool IsFolderItem(AppListItem* item);

// Returns true if the arrow key event should move focus away from the
// |textfield|. This is usually when the insertion point would move away from
// text.
ASH_EXPORT bool LeftRightKeyEventShouldExitText(views::Textfield* textfield,
                                                const ui::KeyEvent& key_event);

// Processes left/right key traversal for the given |textfield|. Returns true
// if focus is moved.
ASH_EXPORT bool ProcessLeftRightKeyTraversalForTextfield(
    views::Textfield* textfield,
    const ui::KeyEvent& key_event);

// Returns a new image with the `icon` atop a circle background with
// `background_color`.
ASH_EXPORT gfx::ImageSkia CreateIconWithCircleBackground(
    const gfx::ImageSkia& icon,
    SkColor background_color);

// Paints a rounded focus bar on the left edge of |canvas|.
ASH_EXPORT void PaintFocusBar(gfx::Canvas* canvas,
                              const gfx::Point content_origin,
                              const int height);

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_UTIL_H_

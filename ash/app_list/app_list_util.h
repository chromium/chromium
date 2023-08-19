// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_UTIL_H_
#define ASH_APP_LIST_APP_LIST_UTIL_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Canvas;
class ImageSkia;
class Point;
}  // namespace gfx

namespace ui {
class ColorProvider;
class KeyEvent;
}  // namespace ui

namespace views {
class Textfield;
class View;
}  // namespace views

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

// Returns true if the `item` is a folder item.
ASH_EXPORT bool IsFolderItem(AppListItem* item);

// Returns true if the arrow key event should move focus away from the
// `textfield`. This is usually when the insertion point would move away from
// text.
ASH_EXPORT bool LeftRightKeyEventShouldExitText(views::Textfield* textfield,
                                                const ui::KeyEvent& key_event);

// Processes left/right key traversal for the given `textfield`. Returns true
// if focus is moved.
ASH_EXPORT bool ProcessLeftRightKeyTraversalForTextfield(
    views::Textfield* textfield,
    const ui::KeyEvent& key_event);

// Returns a new image with the `icon` atop a circle background with
// `background_color`.
ASH_EXPORT gfx::ImageSkia CreateIconWithCircleBackground(
    const gfx::ImageSkia& icon,
    const ui::ColorProvider* color_provider);

// Paints a rounded focus bar on `canvas` starting at `content_origin` extending
// `height` dips vertically.
ASH_EXPORT void PaintFocusBar(gfx::Canvas* canvas,
                              const gfx::Point& content_origin,
                              int height,
                              SkColor color);

// Sets a view as an ignored leaf node, so that it and its child views will be
// ignored by ChromeVox.
ASH_EXPORT void SetViewIgnoredForAccessibility(views::View* view, bool ignored);

// Get the scale factor for the cardified apps grid and app icons.
ASH_EXPORT float GetAppsGridCardifiedScale();

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_UTIL_H_

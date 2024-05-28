// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_TEST_UTIL_H_
#define ASH_PICKER_PICKER_TEST_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
class Clipboard;
}

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

// Returns the text contents of `clipboard`.
ASH_EXPORT std::u16string ReadTextFromClipboard(ui::Clipboard* clipboard);

// Returns the HTML contents of `clipboard`.
ASH_EXPORT std::u16string ReadHtmlFromClipboard(ui::Clipboard* clipboard);

// Returns the filename contents of `clipboard`.
ASH_EXPORT base::FilePath ReadFilenameFromClipboard(ui::Clipboard* clipboard);

// Clicks on `view` with the left mouse button.
void ASH_EXPORT LeftClickOn(views::View& view);

// Presses and releases `key_code` on `widget`.
void ASH_EXPORT PressAndReleaseKey(views::Widget& widget,
                                   ui::KeyboardCode key_code,
                                   int flags = ui::EF_NONE);

}  // namespace ash

#endif  // ASH_PICKER_PICKER_TEST_UTIL_H_

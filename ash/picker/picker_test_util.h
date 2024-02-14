// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_TEST_UTIL_H_
#define ASH_PICKER_PICKER_TEST_UTIL_H_

#include <string>

#include "ash/ash_export.h"

namespace ui {
class Clipboard;
}

namespace ash {

// Returns the HTML contents of `clipboard`.
ASH_EXPORT std::u16string ReadHtmlFromClipboard(ui::Clipboard* clipboard);

}  // namespace ash

#endif

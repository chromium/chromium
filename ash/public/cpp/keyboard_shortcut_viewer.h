// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_SHORTCUT_VIEWER_H_
#define ASH_PUBLIC_CPP_KEYBOARD_SHORTCUT_VIEWER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Toggle the Keyboard Shortcut Viewer window.
// 1. Show the window if it is not open.
// 2. Activate the window if it is open but not active.
// 3. Close the window if it is open and active.
void ASH_PUBLIC_EXPORT ToggleKeyboardShortcutViewer();

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_KEYBOARD_SHORTCUT_VIEWER_H_

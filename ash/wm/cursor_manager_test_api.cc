// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager_test_api.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/wm/native_cursor_manager_ash.h"

namespace ash {

CursorManagerTestApi::CursorManagerTestApi() = default;

CursorManagerTestApi::~CursorManagerTestApi() = default;

display::Display::Rotation CursorManagerTestApi::GetCurrentCursorRotation()
    const {
  return ShellTestApi().native_cursor_manager_ash()->GetRotation();
}

}  // namespace ash

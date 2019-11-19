// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/cursor_manager_test_api.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/wm/native_cursor_manager_ash.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/display/display.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

CursorManagerTestApi::CursorManagerTestApi(::wm::CursorManager* cursor_manager)
    : cursor_manager_(cursor_manager) {}

CursorManagerTestApi::~CursorManagerTestApi() = default;

// TODO(tdanderson): CursorManagerTestApi may no longer be needed.
ui::CursorSize CursorManagerTestApi::GetCurrentCursorSize() const {
  return cursor_manager_->GetCursorSize();
}

gfx::NativeCursor CursorManagerTestApi::GetCurrentCursor() const {
  return cursor_manager_->GetCursor();
}

display::Display::Rotation CursorManagerTestApi::GetCurrentCursorRotation()
    const {
  return ShellTestApi().native_cursor_manager_ash()->GetRotation();
}

float CursorManagerTestApi::GetCurrentCursorScale() const {
  return ShellTestApi().native_cursor_manager_ash()->GetScale();
}

}  // namespace ash

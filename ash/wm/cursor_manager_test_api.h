// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CURSOR_MANAGER_TEST_API_H_
#define ASH_WM_CURSOR_MANAGER_TEST_API_H_

#include "base/macros.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/display.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
enum class CursorSize;
}

namespace wm {
class CursorManager;
}

namespace ash {

// Use the api in this class to test CursorManager.
class CursorManagerTestApi {
 public:
  explicit CursorManagerTestApi(::wm::CursorManager* cursor_manager);
  ~CursorManagerTestApi();

  ui::CursorSize GetCurrentCursorSize() const;
  gfx::NativeCursor GetCurrentCursor() const;
  display::Display::Rotation GetCurrentCursorRotation() const;

 private:
  ::wm::CursorManager* cursor_manager_;

  DISALLOW_COPY_AND_ASSIGN(CursorManagerTestApi);
};

}  // namespace ash

#endif  // ASH_WM_CURSOR_MANAGER_TEST_API_H_

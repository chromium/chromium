// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CURSOR_MANAGER_TEST_API_H_
#define ASH_WM_CURSOR_MANAGER_TEST_API_H_

#include "ui/display/display.h"

namespace ash {

// Use the api in this class to test CursorManager.
class CursorManagerTestApi {
 public:
  CursorManagerTestApi();
  CursorManagerTestApi(const CursorManagerTestApi&) = delete;
  CursorManagerTestApi& operator=(const CursorManagerTestApi&) = delete;
  ~CursorManagerTestApi();

  display::Display::Rotation GetCurrentCursorRotation() const;
};

}  // namespace ash

#endif  // ASH_WM_CURSOR_MANAGER_TEST_API_H_

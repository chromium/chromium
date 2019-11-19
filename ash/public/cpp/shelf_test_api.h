// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_TEST_API_H_
#define ASH_PUBLIC_CPP_SHELF_TEST_API_H_

#include <memory>

#include "ash/ash_export.h"

namespace views {
class View;
}

namespace ash {

// All methods operate on the shelf on the primary display.
class ASH_EXPORT ShelfTestApi {
 public:
  ShelfTestApi();
  virtual ~ShelfTestApi();

  static std::unique_ptr<ShelfTestApi> Create();

  // Returns true if the shelf is visible (e.g. not auto-hidden).
  virtual bool IsVisible() = 0;

  // Returns true if the shelf alignment is BOTTOM_LOCKED, which is not exposed
  // via prefs.
  virtual bool IsAlignmentBottomLocked() = 0;

  virtual views::View* GetHomeButton() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_TEST_API_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_TEST_API_H_
#define ASH_PUBLIC_CPP_SHELF_TEST_API_H_

#include "ash/ash_export.h"

namespace views {
class View;
}

namespace ash {
struct ScrollableShelfInfo;
struct ShelfState;
struct HotseatInfo;

// All methods operate on the shelf on the primary display.
class ASH_EXPORT ShelfTestApi {
 public:
  ShelfTestApi();
  virtual ~ShelfTestApi();

  // Returns true if the shelf is visible (e.g. not auto-hidden).
  bool IsVisible();

  // Returns true if the shelf alignment is BOTTOM_LOCKED, which is not exposed
  // via prefs.
  bool IsAlignmentBottomLocked();

  views::View* GetHomeButton();

  // Returns ui information of scrollable shelf for the given state. If |state|
  // specifies the scroll distance, the target offset, which is the offset value
  // after scrolling by the distance, is also calculated. It is useful if you
  // want to know the offset before the real scroll starts. Note that this
  // function does not change the scrollable shelf.
  ScrollableShelfInfo GetScrollableShelfInfoForState(const ShelfState& state);

  // Returns ui information of hotseat.
  HotseatInfo GetHotseatInfo();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_TEST_API_H_

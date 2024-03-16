// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_FLOAT_TEST_API_H_
#define ASH_WM_FLOAT_FLOAT_TEST_API_H_

#include "ash/wm/float/float_controller.h"

namespace ash {

// Helper class used by tests to access FloatController's internal elements.
class FloatTestApi {
 public:
  // A class to forcefully disable tuck education while it is alive.
  class ScopedTuckEducationDisabler {
   public:
    ScopedTuckEducationDisabler();
    ScopedTuckEducationDisabler(const ScopedTuckEducationDisabler&) = delete;
    ScopedTuckEducationDisabler& operator=(const ScopedTuckEducationDisabler&) =
        delete;
    ~ScopedTuckEducationDisabler();
  };

  FloatTestApi() = delete;

  static int GetFloatedWindowCounter();
  static int GetFloatedWindowMoveToAnotherDeskCounter();
  static FloatController::MagnetismCorner GetMagnetismCornerForBounds(
      const gfx::Rect& bounds_in_screen);
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_FLOAT_TEST_API_H_

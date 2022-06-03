// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace aura {
class Window;
}

namespace ash {

// Used by Chrome to notify the navigation status change and update back
// gesture contextual nudge Ui in ash.
class ASH_PUBLIC_EXPORT BackGestureContextualNudgeController {
 public:
  BackGestureContextualNudgeController() = default;
  virtual ~BackGestureContextualNudgeController() = default;

  // Called when |window|'s navigation entry changed.
  virtual void NavigationEntryChanged(aura::Window* window) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_H_

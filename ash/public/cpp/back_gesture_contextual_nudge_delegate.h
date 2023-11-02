// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_BACK_GESTURE_CONTEXTUAL_NUDGE_DELEGATE_H_
#define ASH_PUBLIC_CPP_BACK_GESTURE_CONTEXTUAL_NUDGE_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace aura {
class Window;
}

namespace ash {

// Used by BackGestureContextualNudgeController to communicate with chrome
// side BackGestureContextualNudgeDelegate.
class ASH_PUBLIC_EXPORT BackGestureContextualNudgeDelegate {
 public:
  BackGestureContextualNudgeDelegate() = default;
  virtual ~BackGestureContextualNudgeDelegate() = default;

  // Starts tracking navigation status for |window| if applicable.
  virtual void MaybeStartTrackingNavigation(aura::Window* window) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_BACK_GESTURE_CONTEXTUAL_NUDGE_DELEGATE_H_

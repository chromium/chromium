// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_TEST_BACK_GESTURE_CONTEXTUAL_NUDGE_DELEGATE_H_
#define ASH_WM_GESTURES_BACK_GESTURE_TEST_BACK_GESTURE_CONTEXTUAL_NUDGE_DELEGATE_H_

#include "ash/public/cpp/back_gesture_contextual_nudge_delegate.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class BackGestureContextualNudgeController;

class TestBackGestureContextualNudgeDelegate
    : public BackGestureContextualNudgeDelegate {
 public:
  explicit TestBackGestureContextualNudgeDelegate(
      BackGestureContextualNudgeController* controller);
  TestBackGestureContextualNudgeDelegate(
      const TestBackGestureContextualNudgeDelegate&) = delete;
  TestBackGestureContextualNudgeDelegate& operator=(
      const TestBackGestureContextualNudgeDelegate&) = delete;

  ~TestBackGestureContextualNudgeDelegate() override;

  // BackGestureContextualNudgeDelegate:
  void MaybeStartTrackingNavigation(aura::Window* window) override;

 private:
  const raw_ptr<BackGestureContextualNudgeController> controller_;
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_TEST_BACK_GESTURE_CONTEXTUAL_NUDGE_DELEGATE_H_

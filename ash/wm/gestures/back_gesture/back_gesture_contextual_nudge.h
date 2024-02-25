// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

// The class to show the back gesture nudge UI animation or cancel/hide the
// animation.
class ASH_EXPORT BackGestureContextualNudge {
 public:
  // Constructor to create a BackGestureContextualNudge with |callback| to be
  // called after the animation is completed or cancelled.
  explicit BackGestureContextualNudge(base::OnceCallback<void(bool)> callback);
  BackGestureContextualNudge(const BackGestureContextualNudge&) = delete;
  BackGestureContextualNudge& operator=(const BackGestureContextualNudge&) =
      delete;

  ~BackGestureContextualNudge();

  // Cancels the animation if the widget is waiting to be shown or fades out
  // the widget if it's currently in animation.
  void CancelAnimationOrFadeOutToHide();

  // The nudge should be counted as shown if the nudge has finished its sliding-
  // in animation no matter whether its following animations get cancelled or
  // not.
  bool ShouldNudgeCountAsShown() const;

  // Set nudge as shown for testing. Only after nudge is counted as shown,
  // the nudge dismiss metrics can be correctly logged. This is to simulate
  // something happens in the middle of a nudge animation.
  void SetNudgeShownForTesting();

  views::Widget* widget() { return widget_.get(); }

 private:
  class ContextualNudgeView;

  std::unique_ptr<views::Widget> widget_;

  // The pointer to the contents view of |widget_|.
  raw_ptr<ContextualNudgeView> nudge_view_ = nullptr;  // not owned
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_H_

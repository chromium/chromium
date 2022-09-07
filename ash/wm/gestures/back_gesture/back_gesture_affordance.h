// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_AFFORDANCE_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_AFFORDANCE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/widget/widget.h"

namespace ash {

// This class is responsible for creating, painting, and positioning the back
// gesture affordance.
class ASH_EXPORT BackGestureAffordance : public gfx::AnimationDelegate {
 public:
  enum class State { DRAGGING, ABORTING, COMPLETING };

  BackGestureAffordance(const gfx::Point& location,
                        bool dragged_from_splitview_divider = false);
  BackGestureAffordance(BackGestureAffordance&) = delete;
  BackGestureAffordance& operator=(BackGestureAffordance&) = delete;
  ~BackGestureAffordance() override;

  // Updates the drag related properties. Note, |during_reverse_dragging|
  // indicates whether dragging on the negative direction of x-axis currently.
  void Update(int x_drag_amount,
              int y_drag_amount,
              bool during_reverse_dragging);

  // Aborts the affordance and animates it back.
  void Abort();

  // Completes the affordance and fading it out.
  void Complete();

  // Returns true if the affordance is activated, which means the drag can be
  // completed to trigger go back.
  bool IsActivated() const;

  gfx::Rect affordance_widget_bounds_for_testing() {
    return affordance_widget_->GetWindowBoundsInScreen();
  }

 private:
  void CreateAffordanceWidget(const gfx::Point& location);

  void UpdateTransform();
  void SchedulePaint();
  void SetAbortProgress(float progress);
  void SetCompleteProgress(float progress);

  // Helper function that returns the affordance progress on current
  // |x_drag_amount_| and |abort_progress_|.
  float GetAffordanceProgress() const;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Widget of the affordance with AffordanceView as the content.
  std::unique_ptr<views::Widget> affordance_widget_;

  // Values that determine current state of the affordance.
  State state_ = State::DRAGGING;
  float y_drag_progress_ = 0.f;
  float abort_progress_ = 0.f;
  float complete_progress_ = 0.f;

  std::unique_ptr<gfx::LinearAnimation> animation_;

  // Drag distance on the positive direction of x-axis.
  int x_drag_amount_ = 0;

  // True if dragging on the negative direction of x-axis.
  bool during_reverse_dragging_ = false;

  // X-axis drag distance while starting reverse drag.
  int x_drag_amount_on_start_reverse_ = 0;

  // X-offset of the affordance while starting reverse drag.
  float offset_on_start_reverse_ = 0.f;

  // True if started to move the affordance on reverse drag.
  bool started_reverse_ = false;

  // Current x-offset of the affordance.
  float current_offset_ = 0.f;

  // True if dragged from the splitview divider to go back.
  bool dragged_from_splitview_divider_ = false;
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_AFFORDANCE_H_

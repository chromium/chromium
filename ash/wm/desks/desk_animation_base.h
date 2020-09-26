// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ANIMATION_BASE_H_
#define ASH_WM_DESKS_DESK_ANIMATION_BASE_H_

#include "ash/public/cpp/metrics_util.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "ui/compositor/throughput_tracker.h"

namespace ash {

class DesksController;

// An abstract class that handles the shared operations need to be performed
// when doing an animation that causes a desk switch animation. Subclasses
// such as DeskActivationAnimation and DeskRemovalAnimation implement the
// abstract interface of this class to handle the unique operations specific to
// each animation type.
class DeskAnimationBase : public RootWindowDeskSwitchAnimator::Delegate {
 public:
  DeskAnimationBase(DesksController* controller,
                    int ending_desk_index,
                    bool is_continuous_gesture_animation);
  DeskAnimationBase(const DeskAnimationBase&) = delete;
  DeskAnimationBase& operator=(const DeskAnimationBase&) = delete;
  ~DeskAnimationBase() override;

  int ending_desk_index() const { return ending_desk_index_; }

  // Launches the animation. This should be done once all animators
  // are created and added to `desk_switch_animators_`. This is to avoid any
  // potential race conditions that might happen if one animator finished phase
  // (1) of the animation while other animators are still being constructed.
  void Launch();

  // Replaces a current animation with an animation to an adjacent desk. By
  // default returns false as most animations do not support replacing.
  virtual bool Replace(bool moving_left, DesksSwitchSource source);

  // Updates a current animation by shifting its animating layer.
  // |scroll_delta_x| is the amount of scroll change since the last scroll
  // update event. Returns false if the animation does not support updating.
  virtual bool UpdateSwipeAnimation(float scroll_delta_x);

  // Ends a current animation, animating to a desk determined by the
  // implementation. Returns false if the animation does not support ending.
  virtual bool EndSwipeAnimation();

  // RootWindowDeskSwitchAnimator::Delegate:
  void OnStartingDeskScreenshotTaken(int ending_desk_index) override;
  void OnEndingDeskScreenshotTaken() override;
  void OnDeskSwitchAnimationFinished() override;

 protected:
  // Abstract functions that can be overridden by child classes to do different
  // things when phase (1), and phase (3) completes. Note that
  // `OnDeskSwitchAnimationFinishedInternal()` will be called before the desks
  // screenshot layers, stored in `desk_switch_animators_`, are destroyed.
  virtual void OnStartingDeskScreenshotTakenInternal(int ending_desk_index) = 0;
  virtual void OnDeskSwitchAnimationFinishedInternal() = 0;

  // Since performance here matters, we have to use the UMA histograms macros to
  // report the smoothness histograms, but each macro use has to be associated
  // with exactly one histogram name. This function allows subclasses to return
  // a callback that reports the histogram using the macro with their desired
  // name.
  virtual metrics_util::ReportCallback GetReportCallback() const = 0;

  DesksController* const controller_;

  // An animator object per each root. Once all the animations are complete,
  // this list is cleared.
  std::vector<std::unique_ptr<RootWindowDeskSwitchAnimator>>
      desk_switch_animators_;

  // The index of the desk that will be active after this animation ends.
  int ending_desk_index_;

  // True if this animation is a continuous gesture animation. Update and End
  // only work when this is true, and we do not start the animation when
  // OnEndingDeskScreenshotTaken is called.
  const bool is_continuous_gesture_animation_;

  // ThroughputTracker used for measuring this animation smoothness.
  ui::ThroughputTracker throughput_tracker_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ANIMATION_BASE_H_

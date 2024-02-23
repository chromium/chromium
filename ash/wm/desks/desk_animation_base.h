// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_ANIMATION_BASE_H_
#define ASH_WM_DESKS_DESK_ANIMATION_BASE_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/throughput_tracker.h"

namespace ash {

class DesksController;

// An abstract class that handles the shared operations need to be performed
// when doing an animation that causes a desk switch animation. Subclasses
// such as DeskActivationAnimation and DeskRemovalAnimation implement the
// abstract interface of this class to handle the unique operations specific to
// each animation type.
class ASH_EXPORT DeskAnimationBase
    : public RootWindowDeskSwitchAnimator::Delegate {
 public:
  DeskAnimationBase(DesksController* controller,
                    int ending_desk_index,
                    bool is_continuous_gesture_animation);
  DeskAnimationBase(const DeskAnimationBase&) = delete;
  DeskAnimationBase& operator=(const DeskAnimationBase&) = delete;
  ~DeskAnimationBase() override;

  int ending_desk_index() const { return ending_desk_index_; }
  int visible_desk_changes() const { return visible_desk_changes_; }

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

  // Returns true if entering/exiting overview during the animation is allowed.
  virtual bool CanEnterOverview() const;
  virtual bool CanEndOverview() const;

  // RootWindowDeskSwitchAnimator::Delegate:
  void OnStartingDeskScreenshotTaken(int ending_desk_index) override;
  void OnEndingDeskScreenshotTaken() override;
  void OnDeskSwitchAnimationFinished() override;

  void set_finished_callback(base::OnceClosure finished_callback) {
    DCHECK(finished_callback_.is_null());
    finished_callback_ = std::move(finished_callback);
  }

  void set_skip_notify_controller_on_animation_finished_for_testing(bool val) {
    skip_notify_controller_on_animation_finished_for_testing_ = val;
  }

  RootWindowDeskSwitchAnimator* GetDeskSwitchAnimatorAtIndexForTesting(
      size_t index) const;

 protected:
  // This will set `is_overview_toggle_allowed_` before and after calling
  // `ActivateDeskInternal()`, allowing exiting/entering overview during the
  // animation.
  void ActivateDeskDuringAnimation(const Desk* desk,
                                   bool update_window_activation);

  // Immediately switches to the target desk and notifies the desk controller
  // that the animation is done, which will end up deleting `this`.
  void ActivateTargetDeskWithoutAnimation();

  // Returns true if any of the animators have failed, for any reason. In this
  // case, we will abort what we're doing and switch to the target desk without
  // animation.
  bool AnimatorFailed() const;

  // Abstract functions that can be overridden by child classes to do different
  // things when phase (1), and phase (3) completes. Note that
  // `OnDeskSwitchAnimationFinishedInternal()` will be called before the desks
  // screenshot layers, stored in `desk_switch_animators_`, are destroyed.
  virtual void OnStartingDeskScreenshotTakenInternal(int ending_desk_index) = 0;
  virtual void OnDeskSwitchAnimationFinishedInternal() = 0;

  // Since performance here matters, we have to use the UMA histograms macros to
  // report the histograms, but each macro use has to be associated with exactly
  // one histogram name. These functions allow subclasses to return callbacks
  // that report each histogram using the macro with their desired name.
  using LatencyReportCallback =
      base::OnceCallback<void(const base::TimeDelta& latency)>;
  virtual LatencyReportCallback GetLatencyReportCallback() const = 0;
  virtual metrics_util::ReportCallback GetSmoothnessReportCallback() const = 0;

  const raw_ptr<DesksController> controller_;

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

  // Used when the animation is a continuous gesture animation. True when
  // `EndSwipeAnimation()` has been called and a fast swipe was detected, and
  // reset to false if `Replace()` has been called. A fast swipe is one where
  // the user starts and ends the swipe gesture within half a second. If this is
  // false, we do not start the animation when `OnEndingDeskScreenshotTaken` is
  // called.
  bool did_continuous_gesture_end_fast_ = false;

  // Used for metrics collection to track how many desks changes a user has seen
  // during the animation. This is different from the number of desk activations
  // as the user may switch desks but not activate it if the desk already has a
  // screenshot taken. This will only change for an activation animation, not a
  // remove animation.
  int visible_desk_changes_ = 0;

  // Used for allowing us to enter or exit overview during a desk animation. If
  // there is an ongoing desk animation, we want to prevent unwanted exit or
  // enter overview toggling so that we don't end up in a strange or unexpected
  // state. Toggling overview is only allowed when we are doing an internal desk
  // activation, where we manually set the overview states of the old active
  // desk and the new active desk.
  bool is_overview_toggle_allowed_ = false;

  // Used for the Ash.Desks.AnimationLatency.* histograms. Null if no animation
  // is being prepared. In a continuous desk animation, the latency is reported
  // only for the first desk switch, and `launch_time_` is null thereafter.
  base::TimeTicks launch_time_;

  // ThroughputTracker used for measuring this animation smoothness.
  std::optional<ui::ThroughputTracker> throughput_tracker_;

  // If true, do not notify |controller_| when
  // OnDeskSwitchAnimationFinished() is called. This class and
  // DeskController are tied together in production code, but may not be in a
  // test scenario.
  bool skip_notify_controller_on_animation_finished_for_testing_ = false;

  // Callback for when the animation is finished.
  base::OnceClosure finished_callback_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_ANIMATION_BASE_H_

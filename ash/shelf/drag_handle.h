// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DRAG_HANDLE_H_
#define ASH_SHELF_DRAG_HANDLE_H_

#include "ash/ash_export.h"
#include "ash/shelf/contextual_nudge.h"
#include "ash/shelf/contextual_nudge_status_tracker.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

class ASH_EXPORT DragHandle : public views::View,
                              public views::ViewTargeterDelegate,
                              public OverviewObserver,
                              public ShellObserver,
                              public ui::ImplicitAnimationObserver,
                              public SplitViewObserver {
 public:
  DragHandle(int drag_handle_corner_radius, Shelf* shelf);
  DragHandle(const DragHandle&) = delete;
  ~DragHandle() override;

  DragHandle& operator=(const DragHandle&) = delete;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Checks whether the drag handle nudge should be shown. Handles the
  // |show_drag_handle_nudge_timer_|, |nudge_visible_|, and
  // |show_nudge_animation_in_progress_|.
  // Returns whether the nudge will be shown.
  bool MaybeShowDragHandleNudge();

  // Animates drag handle and tooltip for drag handle teaching users that
  // swiping up on will take the user back to the home screen.
  void ShowDragHandleNudge();

  // Schedule showing the drag handle.
  void ScheduleShowDragHandleNudge();

  // Immediately begins the animation to return the drag handle back to its
  // original position and hide the tooltip.
  void HideDragHandleNudge(contextual_tooltip::DismissNudgeReason reason);

  // Called when the window drag from shelf starts or ends. The drag handle
  // contextual nudge will remain visible while the gesture is in progress.
  void SetWindowDragFromShelfInProgress(bool gesture_in_progress);

  void UpdateColor();

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Rect GetAnchorBoundsInScreen() const override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;

  // ShellObserver:
  void OnShellDestroying() override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;

  ContextualNudge* drag_handle_nudge() { return drag_handle_nudge_; }

  bool gesture_nudge_target_visibility() const {
    return gesture_nudge_target_visibility_;
  }

  bool show_nudge_animation_in_progress() const {
    return show_nudge_animation_in_progress_;
  }

  bool has_show_drag_handle_timer_for_testing() {
    return show_drag_handle_nudge_timer_.IsRunning();
  }

  void fire_show_drag_handle_timer_for_testing() {
    show_drag_handle_nudge_timer_.FireNow();
  }

  bool has_hide_drag_handle_timer_for_testing() {
    return hide_drag_handle_nudge_timer_.IsRunning();
  }

  void fire_hide_drag_handle_timer_for_testing() {
    hide_drag_handle_nudge_timer_.FireNow();
  }

 private:
  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Animates tooltip for drag handle gesture.
  void ShowDragHandleTooltip();

  // Helper function to hide the drag handle nudge. Called by
  // |hide_drag_handle_nudge_timer_|.
  void HideDragHandleNudgeHelper(bool hidden_by_tap);

  // Helper function to animate the drag handle for the drag handle gesture
  // contextual nudge.
  void AnimateDragHandleShow();

  // Animates translation of the drag handle by |vertical_offset| over
  // |animation_time| using |strategy|.
  void ScheduleDragHandleTranslationAnimation(
      int vertical_offset,
      base::TimeDelta animation_time,
      gfx::Tween::Type tween_type,
      ui::LayerAnimator::PreemptionStrategy strategy);

  // Handler for tap gesture on the contextual nudge widget. It hides the nudge.
  void HandleTapOnNudge();

  // Stops the timer to show the drag handle nudge.
  void StopDragHandleNudgeShowTimer();

  // Pointer to the shelf that owns the drag handle.
  Shelf* const shelf_;

  // Timer to hide drag handle nudge if it has a timed life.
  base::OneShotTimer hide_drag_handle_nudge_timer_;

  // Timer to animate the drag handle and show the nudge.
  base::OneShotTimer show_drag_handle_nudge_timer_;

  // Tracks the target visibility of the gesture nudge.
  bool gesture_nudge_target_visibility_ = false;

  // Tracks whether the in app shelf to home nudge is animating to the visible
  // state. Set to true when animation starts, set to false when animation
  // completes.
  bool show_nudge_animation_in_progress_ = false;

  // Whether window drag from shelf (i.e. gesture from in-app shelf to home or
  // overview) is currently in progress. If the contextual nudge is shown when
  // the gesture starts, it should remain shown until the gesture ends.
  // Set by ShelfLayoutManager using SetWindowDragFromShelfInProgress().
  bool window_drag_from_shelf_in_progress_ = false;

  // A label used to educate users about swipe gestures on the drag handle.
  ContextualNudge* drag_handle_nudge_ = nullptr;

  std::unique_ptr<Shelf::ScopedAutoHideLock> auto_hide_lock_;

  ScopedObserver<SplitViewController, SplitViewObserver> split_view_observer_{
      this};

  ScopedObserver<OverviewController, OverviewObserver> overview_observer_{this};

  ScopedObserver<Shell,
                 ShellObserver,
                 &Shell::AddShellObserver,
                 &Shell::RemoveShellObserver>
      shell_observer_{this};

  base::WeakPtrFactory<DragHandle> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_DRAG_HANDLE_H_

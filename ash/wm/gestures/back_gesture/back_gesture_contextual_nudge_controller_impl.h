// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_IMPL_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/back_gesture_contextual_nudge_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class BackGestureContextualNudgeDelegate;
class BackGestureContextualNudge;

// The class to decide when to show/hide back gesture contextual nudge.
class ASH_EXPORT BackGestureContextualNudgeControllerImpl
    : public SessionObserver,
      public display::DisplayObserver,
      public wm::ActivationChangeObserver,
      public BackGestureContextualNudgeController,
      public ShelfConfig::Observer {
 public:
  BackGestureContextualNudgeControllerImpl();
  BackGestureContextualNudgeControllerImpl(
      const BackGestureContextualNudgeControllerImpl&) = delete;
  BackGestureContextualNudgeControllerImpl& operator=(
      const BackGestureContextualNudgeControllerImpl&) = delete;

  ~BackGestureContextualNudgeControllerImpl() override;

  // Calls when the user starts perform back gesture. We'll cancel the animation
  // if the back nudge is waiting to be shown or is showing.
  void OnBackGestureStarted();

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // BackGestureContextualNudgeController:
  void NavigationEntryChanged(aura::Window* window) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  bool is_monitoring_windows() const { return is_monitoring_windows_; }
  BackGestureContextualNudge* nudge() { return nudge_.get(); }
  BackGestureContextualNudgeDelegate* nudge_delegate() {
    return nudge_delegate_.get();
  }

  base::OneShotTimer* auto_show_timer_for_testing() {
    return &auto_show_timer_;
  }

 private:
  // Returns true if we can show back gesture contextual nudge ui in current
  // configuration.
  // |recheck_delay| - If the nudge should not be shown, it will be set to the
  // delay after which the nudge availability should be checked again.
  bool CanShowNudge(base::TimeDelta* recheck_delay) const;

  // Maybe show nudge ui on top of |window|.
  void MaybeShowNudgeUi(aura::Window* window);

  // Starts or stops monitoring windows activation changes to decide if and when
  // to show up the contextual nudge ui.
  // |can_show_nudge_immediately| - If the nudge can be shown at this point,
  // whether the controller should attempt to show it immediately, or wait for a
  // window navigation (this should be set if monitoring is updated due to a
  // user action rather than a timer callback).
  // Dismiss nudge metrics should be recorded before calling
  // UpdateWindowMonitoring.
  void UpdateWindowMonitoring(bool can_show_nudge_immediately);

  // Callback function to be called after nudge animation is cancelled or
  // completed. |animation_completed| is true when the nudge animation has
  // completed (i.e., finishes the entire animation sequence, which includes
  // sliding in, bouncing and sliding out animation.)
  void OnNudgeAnimationFinished(bool animation_completed);

  // Do necessary cleanup when |this| is destroyed or system is shutdown.
  void DoCleanUp();

  ScopedSessionObserver session_observer_{this};
  display::ScopedDisplayObserver display_observer_{this};

  // The delegate to monitor the current active window's navigation status.
  std::unique_ptr<BackGestureContextualNudgeDelegate> nudge_delegate_;

  // The nudge widget.
  std::unique_ptr<BackGestureContextualNudge> nudge_;

  // True if we're currently monitoring window activation changes.
  bool is_monitoring_windows_ = false;

  // Tracks the visibility of shelf controls.
  bool shelf_control_visible_ = false;

  // Timer to automatically show nudge ui in 24 hours.
  base::OneShotTimer auto_show_timer_;

  base::WeakPtrFactory<BackGestureContextualNudgeControllerImpl>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_IMPL_H_

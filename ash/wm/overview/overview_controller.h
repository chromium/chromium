// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/delayed_animation_observer.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session_metrics_recorder.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_window_occlusion_calculator.h"
#include "ash/wm/raster_scale/raster_scale_controller.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class OverviewSession;

// Manages a overview session which displays an overview of all windows and
// allows selecting a window to activate it.
class ASH_EXPORT OverviewController : public OverviewDelegate,
                                      public wm::ActivationChangeObserver {
 public:
  // `ScopedOcclusionPauser` pauses occlusion tracking for overview mode
  // purposes until it is destroyed. When it is destroyed, occlusion tracking
  // will be unpaused after the given unpause delay. If a
  // `ScopedOcclusionPauser`s is destroyed while an unpause delay is in
  // progress, that delay will be cancelled and the new delay will be used. This
  // means that if two `ScopedOcclusionPauser`s are destroyed, the delay for the
  // second `ScopedOcclusionPauser` to be destroyed will be used, even if the
  // first delay is much longer.
  class ScopedOcclusionPauser {
   public:
    ScopedOcclusionPauser(ScopedOcclusionPauser&&);
    ScopedOcclusionPauser& operator=(ScopedOcclusionPauser&&);

    ScopedOcclusionPauser(const ScopedOcclusionPauser&) = delete;
    ScopedOcclusionPauser& operator=(const ScopedOcclusionPauser&) = delete;

    ~ScopedOcclusionPauser();

   private:
    friend class OverviewController;

    ScopedOcclusionPauser(base::WeakPtr<OverviewController> controller,
                          base::TimeDelta unpause_delay);

    base::WeakPtr<OverviewController> controller_;
    base::TimeDelta unpause_delay_;
  };

  OverviewController();

  OverviewController(const OverviewController&) = delete;
  OverviewController& operator=(const OverviewController&) = delete;

  ~OverviewController() override;

  [[nodiscard]] ScopedOcclusionPauser PauseOcclusionTracker(
      base::TimeDelta unpause_delay);

  // Convenience function to get the overview controller instance, which is
  // created and owned by Shell.
  static OverviewController* Get();

  OverviewSession* overview_session() { return overview_session_.get(); }

  bool disable_app_id_check_for_saved_desks() const {
    return disable_app_id_check_for_saved_desks_;
  }

  bool is_continuous_scroll_in_progress() const {
    return is_continuous_scroll_in_progress_;
  }

  bool windows_have_snapshot() const { return windows_have_snapshot_; }

  // Starts/Ends overview with `type`. Returns true if enter or exit overview
  // successful. Depending on `type` the enter/exit animation will look
  // different. `start_action`/`end_action` is used by UMA to record the reasons
  // that trigger overview starts or ends. E.g, pressing the overview button.
  bool StartOverview(
      OverviewStartAction start_action,
      OverviewEnterExitType type = OverviewEnterExitType::kNormal);
  bool EndOverview(OverviewEndAction end_action,
                   OverviewEnterExitType type = OverviewEnterExitType::kNormal);

  // Returns true if it's possible to enter overview mode in the current
  // configuration. This can be false at certain times, such as when the lock
  // screen is visible we can't enter overview mode.
  bool CanEnterOverview() const;

  // Returns true if overview mode is active.
  bool InOverviewSession() const;

  // Receives a continuous scroll event from the gesture handler and either
  // initializes overview mode in preparation for future continuous scrolls, or
  // immediately calls `OverviewGrid::PositionWindowsForContinuousScrolls()` if
  // overview mode has already been initialized.
  bool HandleContinuousScroll(float y_offset, OverviewEnterExitType type);

  // Moves the current selection forward or backward.
  void IncrementSelection(bool forward);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Returns true if we're in start-overview animation.
  bool IsInStartAnimation();

  // Returns true if overview has been shutdown, but is still animating to the
  // end state ui.
  bool IsCompletingShutdownAnimations() const;

  void AddObserver(OverviewObserver* observer);
  void RemoveObserver(OverviewObserver* observer);

  // Post a task to update the shadow and rounded corners of overview windows.
  void DelayedUpdateRoundedCornersAndShadow();

  // OverviewDelegate:
  void AddExitAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation) override;
  void RemoveAndDestroyExitAnimationObserver(
      DelayedAnimationObserver* animation) override;
  void AddEnterAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override;
  void RemoveAndDestroyEnterAnimationObserver(
      DelayedAnimationObserver* animation_observer) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gained_active,
                          aura::Window* lost_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}

  base::AutoReset<bool> SetDisableAppIdCheckForTests();

  void set_occlusion_pause_duration_for_start_for_test(
      base::TimeDelta duration) {
    occlusion_pause_duration_for_start_ = duration;
  }
  void set_occlusion_pause_duration_for_end_for_test(base::TimeDelta duration) {
    occlusion_pause_duration_for_end_ = duration;
  }
  void set_delayed_animation_task_delay_for_test(base::TimeDelta delta) {
    delayed_animation_task_delay_ = delta;
  }

  void set_windows_have_snapshot_for_test(bool windows_have_snapshot) {
    windows_have_snapshot_ = windows_have_snapshot;
  }

 private:
  // Toggle overview mode. Depending on |type| the enter/exit animation will
  // look different.
  void ToggleOverview(
      OverviewEnterExitType type = OverviewEnterExitType::kNormal);

  // Returns true if it's possible to exit overview mode in the current
  // configuration. This can be false at certain times, such as when the divider
  // or desks are animating.
  bool CanEndOverview(OverviewEnterExitType type) const;

  void OnStartingAnimationComplete(bool canceled);
  void OnEndingAnimationComplete(bool canceled);

  void UpdateRoundedCornersAndShadow();

  // Pause or unpause the occlusion tracker. Resets the unpause delay if we were
  // already in the process of unpausing.
  void MaybePauseOcclusionTracker();
  void MaybeUnpauseOcclusionTracker(base::TimeDelta delay);
  void ResetPauser();

  // Collection of DelayedAnimationObserver objects that own widgets that may be
  // still animating after overview mode ends. If shell needs to shut down while
  // those animations are in progress, the animations are shut down and the
  // widgets destroyed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> delayed_animations_;
  // Collection of DelayedAnimationObserver objects. When this becomes empty,
  // notify shell that the starting animations have been completed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> start_animations_;

  // Indicates that overview shall gain focus when the starting animations have
  // completed.
  bool should_focus_overview_ = false;

  // Used when feature ContinuousOverviewScrollAnimation is enabled to
  // determine the start/end positions of overview items as well as their shadow
  // bounds and corner radii during a continuous scroll. It's true only if the
  // last scroll event was the start of a continuous scroll or a continuous
  // scroll update that is within the threshold.
  bool is_continuous_scroll_in_progress_ = false;

  // We may pause occlusion tracking on enter and exit overview mode.
  std::optional<ScopedOcclusionPauser> enter_pauser_;
  std::optional<ScopedOcclusionPauser> exit_pauser_;

  // The following state tracks occlusion pausing and its delayed unpausing for
  // overview mode.
  int pause_count_ = 0;
  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      occlusion_tracker_pauser_;
  base::CancelableOnceClosure reset_pauser_task_;

  // In order to guarantee relative ordering between occlusion updates and
  // raster scale updates, we need to pause raster scale updates sometimes.
  std::optional<ScopedPauseRasterScaleUpdates> raster_scale_pauser_;

  std::unique_ptr<OverviewSession> overview_session_;

  base::Time last_overview_session_time_;

  base::TimeDelta occlusion_pause_duration_for_start_;
  base::TimeDelta occlusion_pause_duration_for_end_;

  // App dragging enters overview right away. This task is used to delay the
  // |OnStartingAnimationComplete| call so that some animations do not make the
  // initial setup less performant.
  base::TimeDelta delayed_animation_task_delay_;

  base::ObserverList<OverviewObserver> observers_;

  std::unique_ptr<views::Widget::PaintAsActiveLock> paint_as_active_lock_;

  // In ash unittests, the `FullRestoreSaveHandler` isn't hooked up so
  // initialized windows lack an app id. If a window doesn't have a valid app
  // id, then it won't be tracked by `OverviewGrid` as a supported window and
  // those windows will be deemed unsupported for Saved Desks. If
  // `disable_app_id_check_for_saved_desks_` is true, then this check is
  // omitted so we can test Saved Desks.
  bool disable_app_id_check_for_saved_desks_ = false;

  // True if windows shown in overview mode will have a snapshot available.
  // If a snapshot is available then we can pause occlusion tracking until
  // overview mode as finished its enter animation. Otherwise, we must mark
  // all windows as visible immediately.
  bool windows_have_snapshot_ = false;

  std::optional<OverviewSessionMetricsRecorder> session_metrics_recorder_;

  OverviewWindowOcclusionCalculator overview_window_occlusion_calculator_;

  base::WeakPtrFactory<OverviewController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_

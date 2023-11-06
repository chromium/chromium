// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_

#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/wm_metrics.h"
#include "base/scoped_observation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/presentation_time_recorder.h"

namespace ash {

class AutoSnapController;

// Defines two ways to get the split overview session:
// 1. Snap a window and partial overview will automatically show on the other
// side of the screen. Currently behind the feature flag of `kSnapGroup` arm 1
// or `kFasterSplitScreenSetup`;
// 2. In overview session, manually snap a window.
enum class SplitViewOverviewSetupType {
  kSnapThenAutomaticOverview,
  kOverviewThenManualSnap,
  kMaxValue = kOverviewThenManualSnap,
};

// Enumeration of the exit point of the `SplitViewOverviewSession`.
// Please keep in sync with "OverviewEndAction" in
// tools/metrics/histograms/enums.xml.
enum class SplitViewOverviewSessionExitPoint {
  kCompleteByActivating,
  kSkip,
  kWindowDestroy,
  kShutdown,
  kUnspecified,
  kMaxValue = kUnspecified,
};

// Encapsulates the split view state with a single snapped window and
// overview, also known as intermediate split view or the snap group creation
// session.
//
// While `this` is alive, both split view and overview will be active;
// however, the converse is not always true. `this` will automatically be
// destroyed upon split view or overview ending.
//
// There may be at most one SplitViewOverviewSession per root window. Consumers
// should create and manage this via the
// `RootWindowController::ForWindow(aura::Window*)` function.
//
// Note that clamshell split view does *not* have a divider, and resizing
// overview is done via resizing the window directly.
class ASH_EXPORT SplitViewOverviewSession : public aura::WindowObserver,
                                            public WindowStateObserver {
 public:
  SplitViewOverviewSession(aura::Window* window,
                           WindowSnapActionSource snap_action_source);
  SplitViewOverviewSession(const SplitViewOverviewSession&) = delete;
  SplitViewOverviewSession& operator=(const SplitViewOverviewSession&) = delete;
  ~SplitViewOverviewSession() override;

  // Initializes the session by starting overview. This must be called after the
  // constructor, as consumers may check if `this` exists.
  void Init(absl::optional<OverviewStartAction> action,
            absl::optional<OverviewEnterExitType> type);

  // Records the `SplitViewOverviewSessionExitPoint` in uma metrics.
  void RecordSplitViewOverviewSessionExitPointMetrics(
      SplitViewOverviewSessionExitPoint user_action);

  const aura::Window* window() const { return window_; }
  SplitViewOverviewSetupType setup_type() const { return setup_type_; }
  chromeos::WindowStateType GetWindowStateType() const;
  AutoSnapController* auto_snap_controller() {
    return auto_snap_controller_.get();
  }

  // aura::WindowObserver:
  void OnResizeLoopStarted(aura::Window* window) override;
  void OnResizeLoopEnded(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;

  WindowSnapActionSource snap_action_source_for_testing() const {
    return snap_action_source_;
  }

 private:
  // True while we are processing a window resize event.
  bool is_resizing_ = false;

  // Records the presentation time of resize operation in clamshell split view
  // mode.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // Observes windows and performs auto snapping if needed in clamshell mode.
  std::unique_ptr<AutoSnapController> auto_snap_controller_;

  // The single snapped window in intermediate split view, with overview on
  // the opposite side.
  const raw_ptr<aura::Window> window_;

  // True when `this` is being destroyed.
  bool is_shutting_down_ = false;

  SplitViewOverviewSetupType setup_type_ =
      SplitViewOverviewSetupType::kSnapThenAutomaticOverview;

  // Stores the snap action source info for the snapped `window_`.
  const WindowSnapActionSource snap_action_source_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_

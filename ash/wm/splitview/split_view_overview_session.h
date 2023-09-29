// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_

#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/window_state_observer.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/presentation_time_recorder.h"

namespace ash {

class AutoSnapController;

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
class SplitViewOverviewSession : public aura::WindowObserver,
                                 public WindowStateObserver {
 public:
  SplitViewOverviewSession(aura::Window* window);
  SplitViewOverviewSession(const SplitViewOverviewSession&) = delete;
  SplitViewOverviewSession& operator=(const SplitViewOverviewSession&) = delete;
  ~SplitViewOverviewSession() override;

  const aura::Window* window() const { return window_; }
  chromeos::WindowStateType GetWindowStateType() const;

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

 private:
  // Records the presentation time of resize operation in clamshell split view
  // mode.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // Observes windows and performs auto snapping if needed in clamshell mode.
  std::unique_ptr<AutoSnapController> auto_snap_controller_;

  // The single snapped window in intermediate split view, with overview on
  // the opposite side.
  const raw_ptr<aura::Window> window_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_

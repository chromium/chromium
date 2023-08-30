// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_

#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/presentation_time_recorder.h"

namespace ash {

// Encapsulates the clamshell split view state with one snapped window and
// overview, also known as intermediate split view or the
// snap group creation session.
//
// Note that clamshell split view does *not* have a divider, and resizing
// overview is done via resizing the window directly.
//
// TODO(sophiewen): Consider renaming this to ClamshellSplitViewSession.
class SplitViewOverviewSession : public aura::WindowObserver {
 public:
  SplitViewOverviewSession(aura::Window* window);
  SplitViewOverviewSession(const SplitViewOverviewSession&) = delete;
  SplitViewOverviewSession& operator=(const SplitViewOverviewSession&) = delete;
  ~SplitViewOverviewSession() override;

  // aura::WindowObserver:
  void OnResizeLoopStarted(aura::Window* window) override;
  void OnResizeLoopEnded(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

 private:
  // Records the presentation time of resize operation in clamshell split view
  // mode.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_OVERVIEW_SESSION_H_

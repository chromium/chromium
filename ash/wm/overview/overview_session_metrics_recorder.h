// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_SESSION_METRICS_RECORDER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_SESSION_METRICS_RECORDER_H_

#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace ui {
class PresentationTimeRecorder;
}  // namespace ui

namespace ash {

class OverviewController;
class OverviewSession;

// Records metrics and trace events for an individual overview mode session.
// Constructed at the beginning of a session and destroyed at the end. Prefer
// placing new metrics here when possible to avoid crowding
// `OverviewController`.
class OverviewSessionMetricsRecorder : public OverviewObserver {
 public:
  OverviewSessionMetricsRecorder(OverviewStartAction start_action,
                                 OverviewController* controller);
  OverviewSessionMetricsRecorder(const OverviewSessionMetricsRecorder&) =
      delete;
  OverviewSessionMetricsRecorder& operator=(
      const OverviewSessionMetricsRecorder&) = delete;
  ~OverviewSessionMetricsRecorder() override;

  // Called before any overview initialization code is run.
  void OnOverviewSessionInitializing();

  // Called after `OverviewSession::Init()` is complete.
  void OnOverviewSessionInitialized(OverviewSession* session);

  // Called before any overview teardown code is run.
  void OnOverviewSessionEnding();

 private:
  // OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool canceled) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  bool IsDeskBarOpen() const;
  bool IsRenderingDeskBarWithMiniViews() const;

  const OverviewStartAction start_action_;

  std::unique_ptr<ui::PresentationTimeRecorder>
      enter_presentation_time_recorder_;

  raw_ptr<OverviewSession> session_ = nullptr;

  // When entering overview, records whether the desk bar was shown immediately
  // in the first frame (as opposed to after the animation completes or not at
  // all).
  bool desk_bar_shown_immediately_ = false;

  bool has_finished_exit_overview_trace_event_ = false;

  base::Time overview_start_time_;

  base::ScopedObservation<OverviewController, OverviewObserver>
      controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_SESSION_METRICS_RECORDER_H_

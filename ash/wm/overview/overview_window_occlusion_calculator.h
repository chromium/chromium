// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WINDOW_OCCLUSION_CALCULATOR_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WINDOW_OCCLUSION_CALCULATOR_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/wm/desks/window_occlusion_calculator.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"

namespace ash {

class OverviewController;

// Owns the `WindowOcclusionCalculator` used during overview mode sessions.
// Responsible for creating and destroying it at the start and end of each
// session.
class ASH_EXPORT OverviewWindowOcclusionCalculator
    : public OverviewObserver,
      public WindowOcclusionCalculator::Observer {
 public:
  explicit OverviewWindowOcclusionCalculator(
      OverviewController* overview_controller);
  OverviewWindowOcclusionCalculator(const OverviewWindowOcclusionCalculator&) =
      delete;
  OverviewWindowOcclusionCalculator& operator=(
      const OverviewWindowOcclusionCalculator&) = delete;
  ~OverviewWindowOcclusionCalculator() override;

  // This may return a null pointer if:
  // * The `DeskBarWindowOcclusionOptimization` experiment is disabled.
  // * An overview session is not active or is in the process of ending.
  base::WeakPtr<WindowOcclusionCalculator> GetCalculator();

 private:
  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeStartingAnimationComplete(bool canceled) override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;

  // WindowOcclusionCalculator::Observer:
  // Intentionally a no-op. See comments in implementation file.
  void OnWindowOcclusionChanged(aura::Window* window) override {}

  void ComputeOcclusionStateForAllDesks();

  std::optional<WindowOcclusionCalculator> calculator_;
  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      enter_overview_pause_;
  base::ScopedObservation<OverviewController, OverviewObserver>
      overview_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WINDOW_OCCLUSION_CALCULATOR_H_

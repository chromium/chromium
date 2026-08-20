// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WINDOW_OCCLUSION_CALCULATOR_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WINDOW_OCCLUSION_CALCULATOR_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/wm/desks/desks_window_occlusion_calculator.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"

namespace ash {

class OverviewController;

// Owns the `DesksWindowOcclusionCalculator` used during overview mode sessions.
// Responsible for creating and destroying it at the start and end of each
// session.
class ASH_EXPORT OverviewWindowOcclusionCalculator : public OverviewObserver {
 public:
  explicit OverviewWindowOcclusionCalculator(
      OverviewController* overview_controller);
  OverviewWindowOcclusionCalculator(const OverviewWindowOcclusionCalculator&) =
      delete;
  OverviewWindowOcclusionCalculator& operator=(
      const OverviewWindowOcclusionCalculator&) = delete;
  ~OverviewWindowOcclusionCalculator() override;

  // This may return a null pointer if an overview session is not active or is
  // in the process of ending.
  base::WeakPtr<DesksWindowOcclusionCalculator> GetCalculator();

 private:
  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;

  std::unique_ptr<DesksWindowOcclusionCalculator> calculator_;
  base::ScopedObservation<OverviewController, OverviewObserver>
      overview_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WINDOW_OCCLUSION_CALCULATOR_H_

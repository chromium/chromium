// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MULTI_DISPLAY_MULTI_DISPLAY_METRICS_CONTROLLER_H_
#define ASH_WM_MULTI_DISPLAY_MULTI_DISPLAY_METRICS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "ui/aura/window_tracker.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// This class listens to certain display changes and starts a timer to see if
// users resize or reposition app windows after the display change. These
// metrics are used to see if users like the remapping logic our window manager
// provides.
class ASH_EXPORT MultiDisplayMetricsController
    : public display::DisplayObserver {
 public:
  // Histogram names of the boolean histogram.
  constexpr static char kRotatedHistogram[] =
      "Ash.MultiDisplay.WindowsMovedAfterRemap.DisplayRotated";
  constexpr static char kWorkAreaChangedHistogram[] =
      "Ash.MultiDisplay.WindowsMovedAfterRemap.DisplayWorkAreaChanged";

  MultiDisplayMetricsController();
  MultiDisplayMetricsController(const MultiDisplayMetricsController&) = delete;
  MultiDisplayMetricsController& operator=(
      const MultiDisplayMetricsController&) = delete;
  ~MultiDisplayMetricsController() override;

  // Called from `ToplevelWindowEventHandler` when a window drag has
  // successfully started.
  void OnWindowMovedOrResized(aura::Window* window);

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  friend class MultiDisplayMetricsControllerTest;

  // Type of display change that can trigger some metrics. A different type will
  // be written to a different histogram.
  enum class DisplayChangeType {
    kRotated = 0,
    kWorkArea,
  };

  // Called when the timer has ended. Records histograms.
  void OnTimerFinished();

  void RecordHistogram(bool user_moved_window);

  // The windows that were open at the time of the display change. Empty if the
  // timer is not running.
  aura::WindowTracker windows_;

  DisplayChangeType last_display_change_type_ = DisplayChangeType::kRotated;

  // Runs for one minute after a display change of interest. Records histograms
  // on timer end.
  base::OneShotTimer timer_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_MULTI_DISPLAY_MULTI_DISPLAY_METRICS_CONTROLLER_H_

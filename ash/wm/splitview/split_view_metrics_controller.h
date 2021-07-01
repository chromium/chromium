// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_METRICS_CONTROLLER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_METRICS_CONTROLLER_H_

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}  //  namespace aura

namespace ash {
class SplitViewController;

////////////////////////////////////////////////////////////////////////////////
// SplitViewMetricsController:
// Manages split view related metrics. Tablet mode split view and clamshell
// split view with overview next to a snapped window are managed by
// |SplitViewController|. The UMA can be recorded at corresponding points. There
// is another clamshell split view state which has two snapped windows on both
// sides. |SplitViewMetricsController| also inspects the entry and exit point of
// this type of split view state and records related UMA.
class SplitViewMetricsController : public TabletModeObserver,
                                   public SplitViewObserver,
                                   public display::DisplayObserver {
 public:
  // Enumeration of device mode when entering split view.
  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class DeviceUIMode {
    kClamshell,
    kTablet,
    kMaxValue = kTablet,
  };

  // Enumeration of device orientation when entering and using split view.
  // Note that these values are persisted to histograms so existing values
  // should remain unchanged and new values should be added to the end.
  enum class DeviceOrientation {
    // Left and right.
    kLandscape,
    // Top and bottom.
    kPortrait,
    kMaxValue = kPortrait,
  };

  // static
  static SplitViewMetricsController* Get(aura::Window* window);

  // |SplitViewMetricsController| is attached to a |SplitViewController| with
  // the same root window.
  explicit SplitViewMetricsController(
      SplitViewController* split_view_controller);
  SplitViewMetricsController(const SplitViewMetricsController&) = delete;
  SplitViewMetricsController& operator=(const SplitViewMetricsController&) =
      delete;

  ~SplitViewMetricsController() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewWindowResized() override;
  void OnSplitViewWindowSwapped() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  // Calls when entering the split view mode.
  void OnSplitViewStarted();
  // Calls when exiting the split view mode.
  void OnSplitViewEnded();

  // Resets the variables related to time and counter metrics.
  void ResetTimeAndCounter();

  // Checks if we are recording clamshell/tablet mode metrics.
  bool IsRecordingClamshellMetrics() const;
  bool IsRecordingTabletMetrics() const;

  // Reports the engagement metrics for both clamshell and tablet split view.
  void StartRecordClamshellSplitView();
  void StopRecordClamshellSplitView();
  void StartRecordTabletSplitView();
  void StopRecordTabletSplitView();

  // Reports the engagement metrics for both multi-display clamshell and tablet
  // split view. Note that the multi-display mode is managed by the split view
  // metrics controllers of all root windows:
  // - After a root window enters split view, the total number of root windows
  //   in split view becomes two, indicating that multi-display split view
  //   started.
  // - After a root window exits split view, the total number of root windows in
  //   split view becomes one, indicating that multi-display split view ended.
  // - In any time, the total number of root windows in split view larger than
  //   one indicating that it is in the multi-display split view.
  void StartRecordClamshellMultiDisplaySplitView();
  void StopRecordClamshellMultiDisplaySplitView();
  void StartRecordTabletMultiDisplaySplitView();
  void StopRecordTabletMultiDisplaySplitView();

  // We need to save an ptr of the observed |SplitViewController|. Because the
  // |RootWindowController| will be deconstructed in advance. Then, we cannot
  // use it to get observed |SplitViewController|.
  SplitViewController* split_view_controller_ = nullptr;

  // Used to track the change of device orientation.
  DeviceOrientation orientation_ = DeviceOrientation::kLandscape;

  // Start time of clamshell and tablet split view. When stop recording, the
  // start time will be set to |base::TimeTicks::Max()|. This is also used as an
  // indicator of whether we are recording clamshell/tablet split view.
  base::TimeTicks clamshell_split_view_start_time_;
  base::TimeTicks tablet_split_view_start_time_;

  // Counter of resizing windows in split view.
  int tablet_resize_count_ = 0;
  int clamshell_resize_count_ = 0;

  // |TabletModeController| is destroyed before |SplitViewMetricsController|.
  // Sets a |ScopedObservation| to help remove observer.
  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_controller_observation_{this};

  // Counter of swapping windows in split view.
  int swap_count_ = 0;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_METRICS_CONTROLLER_H_

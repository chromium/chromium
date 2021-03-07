// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_
#define CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "ui/display/display_observer.h"

class ChromeBrowserMainParts;
class UsageScenarioTracker;
class PowerMetricsReporter;

namespace chrome {
void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts);
}

namespace ui {
class InputDeviceEventObserver;
}  // namespace ui

class ChromeBrowserMainExtraPartsMetrics : public ChromeBrowserMainExtraParts,
                                           public display::DisplayObserver {
 public:
  ChromeBrowserMainExtraPartsMetrics();
  ~ChromeBrowserMainExtraPartsMetrics() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;
  void PreMainMessageLoopRun() override;

 private:
#if defined(OS_MAC)
  // Records Mac specific metrics.
  void RecordMacMetrics();
#endif  // defined(OS_MAC)

  // DisplayObserver overrides.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  // If the number of displays has changed, emit a UMA metric.
  void EmitDisplaysChangedMetric();

  // A cached value for the number of displays.
  int display_count_;

  // True iff |this| instance is registered as an observer of the native
  // screen.
  bool is_screen_observer_;

#if defined(USE_OZONE) || defined(USE_X11)
  std::unique_ptr<ui::InputDeviceEventObserver> input_device_event_observer_;
#endif  // defined(USE_OZONE) || defined(USE_X11)

#if defined(OS_MAC) || defined(OS_WIN)
  // Tracks coarse usage scenarios that affect performance during a given
  // interval of time (e.g. navigating to a new page, watching a video). The
  // data tracked by this is used by other classes (see below) to report metrics
  // that correlate performance with usage scenarios, which is necessary to
  // optimize the performance of specific scenarios.
  std::unique_ptr<UsageScenarioTracker> usage_scenario_tracker_;

  // Reports power metrics coupled with the data tracked by
  // |usage_scenario_tracker_|, used to analyze the correlation between usage
  // scenarios and power consumption.
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
#endif  // defined(OS_MAC) || defined (OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsMetrics);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

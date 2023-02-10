// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_
#define CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

#include <stdint.h>

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display_observer.h"

class ChromeBrowserMainParts;

#if !BUILDFLAG(IS_ANDROID)
class BatteryDischargeReporter;
class PowerMetricsReporter;
class ProcessMonitor;
#endif

#if BUILDFLAG(IS_LINUX)
class PressureMetricsReporter;
#endif  // BUILDFLAG(IS_LINUX)

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

  ChromeBrowserMainExtraPartsMetrics(
      const ChromeBrowserMainExtraPartsMetrics&) = delete;
  ChromeBrowserMainExtraPartsMetrics& operator=(
      const ChromeBrowserMainExtraPartsMetrics&) = delete;

  ~ChromeBrowserMainExtraPartsMetrics() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PostCreateMainMessageLoop() override;
  void PreProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;
  void PreMainMessageLoopRun() override;

 private:
#if BUILDFLAG(IS_MAC)
  // Records Mac specific metrics.
  void RecordMacMetrics();
#endif  // BUILDFLAG(IS_MAC)

  // DisplayObserver overrides.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // If the number of displays has changed, emit a UMA metric.
  void EmitDisplaysChangedMetric();

  // A cached value for the number of displays.
  int display_count_;

  absl::optional<display::ScopedDisplayObserver> display_observer_;

#if BUILDFLAG(IS_OZONE)
  std::unique_ptr<ui::InputDeviceEventObserver> input_device_event_observer_;
#endif  // BUILDFLAG(IS_OZONE)

#if !BUILDFLAG(IS_ANDROID)
  // The process monitor instance. Allows collecting metrics about every child
  // process.
  std::unique_ptr<ProcessMonitor> process_monitor_;

  // Reports power metrics.
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;

  std::unique_ptr<BatteryDischargeReporter> battery_discharge_reporter_;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX)
  // Reports pressure metrics.
  std::unique_ptr<PressureMetricsReporter> pressure_metrics_reporter_;
#endif  // BUILDFLAG(IS_LINUX)
};

#endif  // CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

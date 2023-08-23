// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_
#define CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile_manager.h"
#endif

class ChromeBrowserMainParts;
class PrefRegistrySimple;
class PrefService;

#if !BUILDFLAG(IS_ANDROID)
class BatteryDischargeReporter;
class PowerMetricsReporter;
class ProcessMonitor;
#endif

#if BUILDFLAG(IS_LINUX)
class PressureMetricsReporter;
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID)
bool IsBundleForMixedDeviceAccordingToVersionCode(
    const std::string& version_code);
#endif

namespace chrome {
void AddMetricsExtraParts(ChromeBrowserMainParts* main_parts);
}

namespace ui {
class InputDeviceEventObserver;
}  // namespace ui

class ChromeBrowserMainExtraPartsMetrics : public ChromeBrowserMainExtraParts,
                                           public display::DisplayObserver,
                                           public ProfileManagerObserver {
 public:
  ChromeBrowserMainExtraPartsMetrics();

  ChromeBrowserMainExtraPartsMetrics(
      const ChromeBrowserMainExtraPartsMetrics&) = delete;
  ChromeBrowserMainExtraPartsMetrics& operator=(
      const ChromeBrowserMainExtraPartsMetrics&) = delete;

  ~ChromeBrowserMainExtraPartsMetrics() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreCreateThreads() override;
  void PostCreateMainMessageLoop() override;
  void PreProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // The --enable-benchmarking flag is transient and should go away after 3
  // launches. This method handles both the countdown and the reset. This logic
  // introduces an implicit dependency between the implementation of
  // chrome://flags and the storage layer in flags_ui::PrefServiceFlagsStorage.
  // This was deemed better than exposing one-off logic into the
  // flags_ui::PrefServiceFlagsStorage layer to handle this use case.
  //
  // |pref_service| is used to store the countdown state. |storage| is used to
  // check whether --enable-benchmarking flag has been enabled, and to later
  // reset the flag if necessary. |access| is unused.
  //
  // Protected for testing.
  static void HandleEnableBenchmarkingCountdown(
      PrefService* pref_service,
      std::unique_ptr<flags_ui::FlagsStorage> storage,
      flags_ui::FlagAccess access);

  // This method asynchronously invokes HandleEnableBenchmarkingCountdown with
  // parameters (fetched asynchronously).
  //
  // Protected for testing.
  virtual void HandleEnableBenchmarkingCountdownAsync();

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, we must wait for post login to have a valid browser Profile*.
  void OnProfileAdded(Profile* profile) override;
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::ScopedObservation<ProfileManager, ChromeBrowserMainExtraPartsMetrics>
      profile_manager_observation_{this};
#endif
};

#endif  // CHROME_BROWSER_METRICS_CHROME_BROWSER_MAIN_EXTRA_PARTS_METRICS_H_

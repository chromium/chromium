// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "ui/base/idle/idle.h"

namespace smart_restart {
namespace {

// Returns true if there are zero tabbed browser windows.
bool IsZeroTabbedBrowserCount() {
  bool is_zero = true;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&is_zero](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
          is_zero = false;
          return false;
        }
        return true;
      });
  return is_zero;
}

void RecordDuration(std::string_view histogram_name, base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(histogram_name, duration, base::Seconds(1),
                                base::Days(1), 50);
}

const char* GetDurationSuffix(base::TimeDelta duration) {
  if (duration < base::Minutes(1)) {
    return ".Under1Min";
  }
  if (duration < base::Minutes(5)) {
    return ".1To5Min";
  }
  if (duration < base::Minutes(10)) {
    return ".5To10Min";
  }
  return ".Over10Min";
}

void RecordRestartabilitySnapshot(
    std::string_view histogram_name_prefix,
    base::TimeDelta update_duration,
    const std::optional<smart_restart::RestartabilityState>& snapshot) {
  if (!snapshot.has_value()) {
    return;
  }
  std::string histogram_name =
      base::StrCat({histogram_name_prefix, GetDurationSuffix(update_duration)});
  base::UmaHistogramExactLinear(
      histogram_name, snapshot.value().GetRestartabilityStateFactor(),
      smart_restart::RestartabilityState::SmartRestartStateFactor::kMaxValue +
          1);
}

}  // namespace

SmartRestartMetricsObserver::SmartRestartMetricsObserver(
    UpgradeDetector* upgrade_detector)
    : SmartRestartMetricsObserver(
          upgrade_detector,
          base::BindRepeating(&IsZeroTabbedBrowserCount)) {}

SmartRestartMetricsObserver::SmartRestartMetricsObserver(
    UpgradeDetector* upgrade_detector,
    IsZeroBrowserCallback is_zero_callback)
    : upgrade_detector_(upgrade_detector),
      is_zero_callback_(std::move(is_zero_callback)) {
  upgrade_detector_->AddObserver(this);
#if BUILDFLAG(IS_MAC)
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
#endif
  lock_state_subscription_ = ui::AddScreenLockCallback(
      base::BindRepeating(&SmartRestartMetricsObserver::OnLockStateChanged,
                          base::Unretained(this)));
  // Check initial state.
  if (ui::CheckIdleStateIsLocked()) {
    OnLockStateChanged(true);
  }
}

SmartRestartMetricsObserver::~SmartRestartMetricsObserver() {
  upgrade_detector_->RemoveObserver(this);
  // If the browser process is shutting down, record any pending durations.
#if BUILDFLAG(IS_MAC)
  RecordZeroWindowMetrics();
#endif
  RecordLockedDurationMetrics();
}

void SmartRestartMetricsObserver::OnUpgradeRecommended() {
  if (locked_timer_.has_value() && !locked_update_timer_.has_value()) {
    locked_update_timer_.emplace();
    locked_snapshot_ = RestartabilityMonitor::ComputeCurrentState();
  }
#if BUILDFLAG(IS_MAC)
  if (zero_window_timer_.has_value() &&
      !zero_window_update_timer_.has_value()) {
    zero_window_update_timer_.emplace();
    zero_window_snapshot_ = RestartabilityMonitor::ComputeCurrentState();
  }
#endif
}

void SmartRestartMetricsObserver::SetLockedStateForTesting(bool is_locked) {
  OnLockStateChanged(is_locked);
}

void SmartRestartMetricsObserver::OnLockStateChanged(bool is_locked) {
  if (is_locked && !was_locked_) {
    // Transitioned to LOCKED state. Start timer.
    locked_timer_.emplace();
    if (upgrade_detector_->is_upgrade_available()) {
      locked_update_timer_.emplace();
      locked_snapshot_ = RestartabilityMonitor::ComputeCurrentState();
    }
  } else if (!is_locked && was_locked_) {
    // Transitioned out of LOCKED state. Record duration.
    RecordLockedDurationMetrics();
  }
  was_locked_ = is_locked;
}

void SmartRestartMetricsObserver::RecordLockedDurationMetrics() {
  if (!locked_timer_.has_value()) {
    return;
  }

  // Record the baseline duration (how long users stay in lock state).
  base::TimeDelta duration = locked_timer_->Elapsed();
  RecordDuration("Session.LockedDuration", duration);

  // Record the duration specifically when an update is pending.
  if (locked_update_timer_.has_value()) {
    base::TimeDelta update_duration = locked_update_timer_->Elapsed();
    RecordDuration("Session.LockedDuration.WithUpdate", update_duration);

    // Record the restartability state segmented by duration buckets.
    RecordRestartabilitySnapshot("Session.LockedDuration.RestartabilityV2",
                                 update_duration, locked_snapshot_);
  }
  locked_timer_.reset();
  locked_update_timer_.reset();
  locked_snapshot_.reset();
}

#if BUILDFLAG(IS_MAC)
void SmartRestartMetricsObserver::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  // If we were tracking a zero-window duration, stop now because a window
  // appeared.
  RecordZeroWindowMetrics();
}

void SmartRestartMetricsObserver::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  // If the last browser window is closed, start tracking the duration.
  // Note: On macOS, the application stays running even with zero windows.
  if (is_zero_callback_.Run() && !zero_window_timer_.has_value()) {
    zero_window_timer_.emplace();
    if (upgrade_detector_->is_upgrade_available()) {
      zero_window_update_timer_.emplace();
      zero_window_snapshot_ = RestartabilityMonitor::ComputeCurrentState();
    }
  }
}

void SmartRestartMetricsObserver::RecordZeroWindowMetrics() {
  if (!zero_window_timer_.has_value()) {
    return;
  }

  // Record the baseline duration (how long users stay in Zero Window state).
  base::TimeDelta duration = zero_window_timer_->Elapsed();
  RecordDuration("Session.ZeroWindowDuration", duration);

  // Record the duration specifically when an update is pending.
  if (zero_window_update_timer_.has_value()) {
    base::TimeDelta update_duration = zero_window_update_timer_->Elapsed();
    RecordDuration("Session.ZeroWindowDuration.WithUpdate", update_duration);

    // Record the restartability state segmented by duration buckets.
    RecordRestartabilitySnapshot("Session.ZeroWindowDuration.RestartabilityV2",
                                 update_duration, zero_window_snapshot_);
  }
  zero_window_timer_.reset();
  zero_window_update_timer_.reset();
  zero_window_snapshot_.reset();
}
#endif

}  // namespace smart_restart

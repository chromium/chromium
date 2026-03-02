// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_SMART_RESTART_METRICS_OBSERVER_H_
#define CHROME_BROWSER_LIFETIME_SMART_RESTART_METRICS_OBSERVER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"

class UpgradeDetector;

namespace smart_restart {

// Observes to record metrics related to smart restart opportunities.
class SmartRestartMetricsObserver : public BrowserListObserver,
                                    public UpgradeObserver {
 public:
  explicit SmartRestartMetricsObserver(UpgradeDetector* upgrade_detector);

  // A type of callback that is run to check if the browser is in a zero window
  // state.
  using IsZeroBrowserCallback = base::RepeatingCallback<bool()>;

  SmartRestartMetricsObserver(UpgradeDetector* upgrade_detector,
                              IsZeroBrowserCallback is_zero_callback);
  SmartRestartMetricsObserver(const SmartRestartMetricsObserver&) = delete;
  SmartRestartMetricsObserver& operator=(const SmartRestartMetricsObserver&) =
      delete;
  ~SmartRestartMetricsObserver() override;

#if BUILDFLAG(IS_MAC)
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
#endif

  // For tests to simulate a change in the lock state.
  void SetLockedStateForTesting(bool is_locked);

  // UpgradeObserver:
  void OnUpgradeRecommended() override;

 private:
#if BUILDFLAG(IS_MAC)
  void RecordZeroWindowMetrics();
#endif
  void RecordLockedDurationMetrics();
  void OnLockStateChanged(bool is_locked);

  const raw_ptr<UpgradeDetector> upgrade_detector_;
  IsZeroBrowserCallback is_zero_callback_;
#if BUILDFLAG(IS_MAC)
  std::optional<base::ElapsedTimer> zero_window_timer_;
  std::optional<base::ElapsedTimer> zero_window_update_timer_;
  std::optional<RestartabilityState> zero_window_snapshot_;
#endif
  base::CallbackListSubscription lock_state_subscription_;
  std::optional<base::ElapsedTimer> locked_timer_;
  std::optional<base::ElapsedTimer> locked_update_timer_;
  std::optional<RestartabilityState> locked_snapshot_;
  bool was_locked_ = false;
};

}  // namespace smart_restart

#endif  // CHROME_BROWSER_LIFETIME_SMART_RESTART_METRICS_OBSERVER_H_

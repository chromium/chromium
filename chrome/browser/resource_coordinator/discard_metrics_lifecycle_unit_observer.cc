// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/discard_metrics_lifecycle_unit_observer.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {

DiscardMetricsLifecycleUnitObserver::DiscardMetricsLifecycleUnitObserver() =
    default;
DiscardMetricsLifecycleUnitObserver::~DiscardMetricsLifecycleUnitObserver() =
    default;

void DiscardMetricsLifecycleUnitObserver::OnLifecycleUnitStateChanged(
    LifecycleUnit* lifecycle_unit,
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {
  if (lifecycle_unit->GetState() == LifecycleUnitState::DISCARDED)
    OnDiscard(lifecycle_unit, reason);
  else if (last_state == LifecycleUnitState::DISCARDED)
    OnReload();
}

void DiscardMetricsLifecycleUnitObserver::OnLifecycleUnitDestroyed(
    LifecycleUnit* lifecycle_unit) {
  // If the browser is not shutting down and the tab is loaded after
  // being discarded, record TabManager.Discarding.ReloadToCloseTime.
  if (g_browser_process && !g_browser_process->IsShuttingDown() &&
      lifecycle_unit->GetState() != LifecycleUnitState::DISCARDED &&
      !reload_time_.is_null()) {
    auto reload_to_close_time = NowTicks() - reload_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.ReloadToCloseTime",
                               reload_to_close_time, base::Seconds(1),
                               base::Days(1), 100);
  }

  // This is a self-owned object that destroys itself with the LifecycleUnit
  // that it observes.
  lifecycle_unit->RemoveObserver(this);
  delete this;
}

void DiscardMetricsLifecycleUnitObserver::OnDiscard(
    LifecycleUnit* lifecycle_unit,
    LifecycleUnitStateChangeReason reason) {
  discard_time_ = NowTicks();
  discard_reason_ = reason;
  last_focused_time_before_discard_ = lifecycle_unit->GetLastFocusedTimeTicks();

  static int discard_count = 0;
  UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.DiscardCount",
                              ++discard_count, 1, 1000, 50);
}

void DiscardMetricsLifecycleUnitObserver::OnReload() {
  DCHECK(!discard_time_.is_null());
  reload_time_ = NowTicks();

  static int reload_count = 0;
  UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.ReloadCount",
                              ++reload_count, 1, 1000, 50);
  auto discard_to_reload_time = reload_time_ - discard_time_;
  UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.DiscardToReloadTime",
                             discard_to_reload_time, base::Seconds(1),
                             base::Days(1), 100);
  auto inactive_to_reload_time =
      reload_time_ - last_focused_time_before_discard_;
  UMA_HISTOGRAM_CUSTOM_TIMES("TabManager.Discarding.InactiveToReloadTime",
                             inactive_to_reload_time, base::Seconds(1),
                             base::Days(1), 100);

  if (discard_reason_ == LifecycleUnitStateChangeReason::BROWSER_INITIATED) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "TabManager.Discarding.DiscardToReloadTime.Proactive",
        discard_to_reload_time, base::Seconds(1), base::Days(1), 100);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "TabManager.Discarding.InactiveToReloadTime.Proactive",
        inactive_to_reload_time, base::Seconds(1), base::Days(1), 100);
  }
}

}  // namespace resource_coordinator

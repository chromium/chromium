// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_METRICS_LIFECYCLE_UNIT_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_METRICS_LIFECYCLE_UNIT_OBSERVER_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"

#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"

namespace resource_coordinator {

using ::mojom::LifecycleUnitState;

// Observes a LifecycleUnit to record metrics.
class DiscardMetricsLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  DiscardMetricsLifecycleUnitObserver();

  DiscardMetricsLifecycleUnitObserver(
      const DiscardMetricsLifecycleUnitObserver&) = delete;
  DiscardMetricsLifecycleUnitObserver& operator=(
      const DiscardMetricsLifecycleUnitObserver&) = delete;

  ~DiscardMetricsLifecycleUnitObserver() override;

  // LifecycleUnitObserver:
  void OnLifecycleUnitStateChanged(
      LifecycleUnit* lifecycle_unit,
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason) override;
  void OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit) override;

 private:
  // Invoked when the LifecycleUnit is discarded.
  void OnDiscard(LifecycleUnit* lifecycle_unit,
                 LifecycleUnitStateChangeReason reason);

  // Invoked when the LifecycleUnit is reloaded.
  void OnReload();

  // The last time at which the LifecycleUnit was focused, updated when the
  // LifecycleUnit is discarded.
  base::TimeTicks last_focused_time_before_discard_;

  // The last time at which the LifecycleUnit was discarded.
  base::TimeTicks discard_time_;

  // The last discard reason.
  LifecycleUnitStateChangeReason discard_reason_ =
      LifecycleUnitStateChangeReason::BROWSER_INITIATED;

  // The last time at which the LifecycleUnit was reloaded after being
  // discarded.
  base::TimeTicks reload_time_;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_METRICS_LIFECYCLE_UNIT_OBSERVER_H_

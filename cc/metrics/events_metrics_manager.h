// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENTS_METRICS_MANAGER_H_
#define CC_METRICS_EVENTS_METRICS_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

// Manages a list of active EventMetrics objects. Each thread (main or impl) has
// its own instance of this class to help it determine which events have led to
// a frame update.
class CC_EXPORT EventsMetricsManager {
 public:
  // This interface is used to denote the scope of an event handling. The scope
  // is started as soon as an instance is constructed and ended when instance is
  // desctucted. EventsMetricsManager uses this to determine whether a frame
  // update has happened due to handling of a specific event or not.
  class ScopedMonitor {
   public:
    ScopedMonitor() = default;
    virtual ~ScopedMonitor() = 0;

    ScopedMonitor(const ScopedMonitor&) = delete;
    ScopedMonitor& operator=(const ScopedMonitor&) = delete;
  };

  EventsMetricsManager();
  ~EventsMetricsManager();

  EventsMetricsManager(const EventsMetricsManager&) = delete;
  EventsMetricsManager& operator=(const EventsMetricsManager&) = delete;

  // Called by clients when they start handling an event. Destruction of the
  // scoped monitor indicates the end of event handling. |event_metrics| is
  // allowed to be nullptr in which case the return value would also be nullptr.
  std::unique_ptr<ScopedMonitor> GetScopedMonitor(
      const EventMetrics* event_metrics);

  // Called by clients when a frame needs to be produced. If any scoped monitor
  // is active at this time, its corresponding event metrics would be saved.
  void SaveActiveEventMetrics();

  // Empties the list of saved EventMetrics objects, returning them to the
  // caller.
  std::vector<EventMetrics> TakeSavedEventsMetrics();

  size_t saved_events_metrics_count_for_testing() const {
    return saved_events_.size();
  }

 private:
  void OnScopedMonitorEnded();

  // Current active EventMetrics, if any.
  const EventMetrics* active_event_ = nullptr;

  // List of saved event metrics.
  std::vector<EventMetrics> saved_events_;

  base::WeakPtrFactory<EventsMetricsManager> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_METRICS_EVENTS_METRICS_MANAGER_H_

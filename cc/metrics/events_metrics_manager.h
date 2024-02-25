// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENTS_METRICS_MANAGER_H_
#define CC_METRICS_EVENTS_METRICS_MANAGER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

// Manages a list of active EventMetrics objects. Each thread (main or impl) has
// its own instance of this class to help it determine which events have led to
// a frame update.
class CC_EXPORT EventsMetricsManager {
 public:
  // This interface is used to denote the scope of an event handling. The scope
  // is started as soon as an instance is constructed and ended when the
  // instance is destructed. EventsMetricsManager uses this to determine whether
  // a frame update has happened due to handling of a specific event or not.
  // Since it is possible to have nested event loops, scoped monitors can be
  // nested.
  class ScopedMonitor {
   public:
    // Type of the callback function that is called after a scoped monitor goes
    // out of scope. `handled` specifies whether the corresponding event was
    // handled in which case the function should return its corresponding
    // `EventMetrics` object for reporting purposes. Since calling this callback
    // denotes the end of event processing, clients can use this to set relevant
    // timestamps to the metrics object, before potentially returning it.
    using DoneCallback =
        base::OnceCallback<std::unique_ptr<EventMetrics>(bool handled)>;

    ScopedMonitor();
    virtual ~ScopedMonitor();

    virtual void SetSaveMetrics() = 0;

    ScopedMonitor(const ScopedMonitor&) = delete;
    ScopedMonitor& operator=(const ScopedMonitor&) = delete;
  };

  EventsMetricsManager();
  ~EventsMetricsManager();

  EventsMetricsManager(const EventsMetricsManager&) = delete;
  EventsMetricsManager& operator=(const EventsMetricsManager&) = delete;

  // Called by clients when they start handling an event. Destruction of the
  // scoped monitor indicates the end of event handling which ends in calling
  // `done_callback`. `done_callback` is allowed to be a null callback, which
  // means the client is not interested in reporting metrics for the event being
  // handled in this specific scope.
  std::unique_ptr<ScopedMonitor> GetScopedMonitor(
      ScopedMonitor::DoneCallback done_callback);

  // Called by clients when a frame needs to be produced. If any scoped monitor
  // is active at this time, its corresponding event metrics would be marked to
  // be saved when it goes out of scope.
  void SaveActiveEventMetrics();

  // Empties the list of saved EventMetrics objects, returning them to the
  // caller.
  EventMetrics::List TakeSavedEventsMetrics();

  size_t saved_events_metrics_count_for_testing() const {
    return saved_events_.size();
  }

 private:
  class ScopedMonitorImpl;

  // Called when the most nested scoped monitor is destroyed. If the monitored
  // metrics need to be saved it will be passed in as `metrics`.
  void OnScopedMonitorEnded(std::unique_ptr<EventMetrics> metrics);

  // Stack of active, potentially nested, scoped monitors.
  std::vector<raw_ptr<ScopedMonitorImpl, VectorExperimental>>
      active_scoped_monitors_;

  // List of event metrics saved for reporting.
  EventMetrics::List saved_events_;
};

}  // namespace cc

#endif  // CC_METRICS_EVENTS_METRICS_MANAGER_H_

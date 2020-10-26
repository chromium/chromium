// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/stl_util.h"

namespace cc {
namespace {

// A `ScopedMonitor` implementation that takes a callback and runs it upon
// destruction.
class ScopedMonitorImpl : public EventsMetricsManager::ScopedMonitor {
 public:
  explicit ScopedMonitorImpl(base::OnceClosure done_callback)
      : done_callback_runner_(std::move(done_callback)) {}

 private:
  // Holds a callback closure that is run automatically when the scoped monitor
  // is destroyed.
  base::ScopedClosureRunner done_callback_runner_;
};

}  // namespace

EventsMetricsManager::ScopedMonitor::~ScopedMonitor() = default;

EventsMetricsManager::EventsMetricsManager() = default;
EventsMetricsManager::~EventsMetricsManager() = default;

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
EventsMetricsManager::GetScopedMonitor(const EventMetrics* event_metrics) {
  active_events_.push_back(event_metrics);
  return std::make_unique<ScopedMonitorImpl>(base::BindOnce(
      &EventsMetricsManager::OnScopedMonitorEnded, weak_factory_.GetWeakPtr()));
}

void EventsMetricsManager::SaveActiveEventMetrics() {
  if (active_events_.size() > 0 && active_events_.back()) {
    // TODO(crbug.com/1054009): It is fine to make a copy of active EventMetrics
    // object here as we are not going to change it later. However, the plan is
    // to add timestamp of when the processing is done to this object. Since end
    // of the processing happens after this code, we can't simply make a copy
    // here. In that case, here we can just mark the event for saving and do the
    // actual copy when the scoped monitor is destroyed which happens after the
    // event processing is done.
    saved_events_.push_back(*active_events_.back());

    // The items will be popped from the stack when the scoped monitor is ended,
    // but replace it nullptr here to avoid reporting it multiple times.
    active_events_.back() = nullptr;
  }
}

std::vector<EventMetrics> EventsMetricsManager::TakeSavedEventsMetrics() {
  std::vector<EventMetrics> result;
  result.swap(saved_events_);
  return result;
}

void EventsMetricsManager::OnScopedMonitorEnded() {
  DCHECK_GT(active_events_.size(), 0u);
  active_events_.pop_back();
}

}  // namespace cc

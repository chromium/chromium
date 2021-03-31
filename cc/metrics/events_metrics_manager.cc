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

class EventsMetricsManager::ScopedMonitorImpl
    : public EventsMetricsManager::ScopedMonitor {
 public:
  ScopedMonitorImpl(EventsMetricsManager* manager, DoneCallback done_callback)
      : manager_(manager), done_callback_(std::move(done_callback)) {
    DCHECK_NE(manager, nullptr);
  }

  ~ScopedMonitorImpl() override {
    std::unique_ptr<EventMetrics> metrics;
    if (!done_callback_.is_null()) {
      const bool handled = save_metrics_;
      metrics = std::move(done_callback_).Run(handled);

      // If `handled` is false, the callback should return nullptr.
      DCHECK(handled || !metrics);
    }
    manager_->OnScopedMonitorEnded(std::move(metrics));
  }

  void set_save_metrics() { save_metrics_ = true; }

 private:
  EventsMetricsManager* const manager_;
  DoneCallback done_callback_;
  bool save_metrics_ = false;
};

EventsMetricsManager::ScopedMonitor::~ScopedMonitor() = default;

EventsMetricsManager::EventsMetricsManager() = default;
EventsMetricsManager::~EventsMetricsManager() = default;

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
EventsMetricsManager::GetScopedMonitor(
    ScopedMonitor::DoneCallback done_callback) {
  auto monitor =
      std::make_unique<ScopedMonitorImpl>(this, std::move(done_callback));
  active_scoped_monitors_.push_back(monitor.get());
  return monitor;
}

void EventsMetricsManager::SaveActiveEventMetrics() {
  if (active_scoped_monitors_.size() > 0) {
    // Here we just set the flag to save the active metrics. The actual saving
    // happens when the scoped monitor is destroyed to give clients opportunity
    // to use/update the metrics object until the end of their processing.
    active_scoped_monitors_.back()->set_save_metrics();
  }
}

EventMetrics::List EventsMetricsManager::TakeSavedEventsMetrics() {
  EventMetrics::List result;
  result.swap(saved_events_);
  return result;
}

void EventsMetricsManager::OnScopedMonitorEnded(
    std::unique_ptr<EventMetrics> metrics) {
  DCHECK_GT(active_scoped_monitors_.size(), 0u);
  active_scoped_monitors_.pop_back();

  if (metrics)
    saved_events_.push_back(std::move(metrics));
}

}  // namespace cc

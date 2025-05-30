// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"

namespace cc {

class EventsMetricsManager::ScopedMonitorImpl
    : public EventsMetricsManager::ScopedMonitor {
 public:
  ScopedMonitorImpl(EventsMetricsManager* manager, DoneCallback done_callback)
      : manager_(manager), done_callback_(std::move(done_callback)) {
    DCHECK(manager_);
  }

  ~ScopedMonitorImpl() override {
    if (manager_)
      End();
  }

  void End() {
    DCHECK(manager_);
    std::unique_ptr<EventMetrics> metrics;
    if (!done_callback_.is_null()) {
      const bool handled = save_metrics_;
      metrics = std::move(done_callback_).Run(handled);

      // If `handled` is false, the callback should return nullptr.
      DCHECK(handled || !metrics);
    }
    manager_->OnScopedMonitorEnded(std::move(metrics));
    manager_ = nullptr;
  }

  // Overridden from EventsMetricsManager::ScopedMonitor.
  void SetSaveMetrics() override { save_metrics_ = true; }

 private:
  raw_ptr<EventsMetricsManager> manager_;
  DoneCallback done_callback_;
  bool save_metrics_ = false;
};

EventsMetricsManager::ScopedMonitor::ScopedMonitor() = default;
EventsMetricsManager::ScopedMonitor::~ScopedMonitor() = default;

EventsMetricsManager::EventsMetricsManager() = default;

EventsMetricsManager::~EventsMetricsManager() {
  // If `EventsMetricsManager` is shut down while events are active, end active
  // scoped monitors immediately.
  while (!active_scoped_monitors_.empty())
    active_scoped_monitors_.back()->End();
}

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
    active_scoped_monitors_.back()->SetSaveMetrics();
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

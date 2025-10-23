// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/events_metrics_manager.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "cc/metrics/event_metrics.h"

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

      // If `handled` is false and the metrics don't need to be kept around even
      // though handling the event didn't cause a frame update, the callback
      // should return nullptr unless .
      DCHECK(handled || !metrics ||
             EventMetrics::ShouldKeepEvenWithoutCausingFrameUpdate(
                 metrics->type()));
      if (metrics && !handled) {
        metrics->set_caused_frame_update(false);
      }
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

void EventsMetricsManager::DropSavedEventMetricsForNoFrameUpdate() {
  // First re-arrange `saved_events_` so that:
  //   1. [`saved_events_.begin()`, `first_to_erase`) only contains metrics
  //      which we should keep around even if handling them didn't cause a frame
  //      update.
  //   2. [`first_to_erase`, `saved_events_.end()`) contains all other metrics.
  auto first_to_erase = std::remove_if(
      saved_events_.begin(), saved_events_.end(),
      [](const std::unique_ptr<EventMetrics>& metrics) {
        return !EventMetrics::ShouldKeepEvenWithoutCausingFrameUpdate(
            metrics->type());
      });
  // Then delete the other metrics.
  saved_events_.erase(first_to_erase, saved_events_.end());
  // Finally, mark that the metrics we kept didn't cause a frame update.
  for (auto& kept_event : saved_events_) {
    kept_event->set_caused_frame_update(false);
  }
}

void EventsMetricsManager::OnScopedMonitorEnded(
    std::unique_ptr<EventMetrics> metrics) {
  DCHECK_GT(active_scoped_monitors_.size(), 0u);
  active_scoped_monitors_.pop_back();

  if (metrics) {
    if (metrics->type() == EventMetrics::EventType::kGestureScrollUpdate ||
        metrics->type() == EventMetrics::EventType::kFirstGestureScrollUpdate ||
        metrics->type() ==
            EventMetrics::EventType::kInertialGestureScrollUpdate) {
      auto* scroll_update = metrics->AsScrollUpdate();
      scroll_update->set_did_scroll(did_scroll_);
    }
    saved_events_.push_back(std::move(metrics));
  }
  did_scroll_ = false;
}

}  // namespace cc

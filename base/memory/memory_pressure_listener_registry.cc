// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener_registry.h"

#include <atomic>

#include "base/feature_list.h"
#include "base/memory/memory_pressure_level.h"
#include "base/metrics/field_trial_params.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"

namespace base {

namespace {

std::atomic<bool> g_notifications_suppressed = false;

BASE_FEATURE(kSuppressMemoryListeners,
             "SuppressMemoryListeners",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kSuppressMemoryListenersStart{
    &kSuppressMemoryListeners, "suppress_memory_listeners_start",
    static_cast<int>(base::MemoryPressureListenerTag::kMax)};

const base::FeatureParam<int> kSuppressMemoryListenersEnd{
    &kSuppressMemoryListeners, "suppress_memory_listeners_end",
    static_cast<int>(base::MemoryPressureListenerTag::kMax)};

}  // namespace

MemoryPressureListenerRegistry::MemoryPressureListenerRegistry() = default;

// static
MemoryPressureListenerRegistry& MemoryPressureListenerRegistry::Get() {
  static auto* const registry = new MemoryPressureListenerRegistry();
  return *registry;
}

// static
void MemoryPressureListenerRegistry::NotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_NE(memory_pressure_level, MEMORY_PRESSURE_LEVEL_NONE);
  TRACE_EVENT_INSTANT(
      trace_event::MemoryDumpManager::kTraceCategory,
      "MemoryPressureListener::NotifyMemoryPressure",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(
            trace_event::MemoryPressureLevelToTraceEnum(memory_pressure_level));
      });
  if (AreNotificationsSuppressed()) {
    return;
  }
  Get().DoNotifyMemoryPressure(memory_pressure_level);
}

void MemoryPressureListenerRegistry::AddObserver(
    SyncMemoryPressureListener* listener) {
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
  listeners_.AddObserver(listener);
}

void MemoryPressureListenerRegistry::RemoveObserver(
    SyncMemoryPressureListener* listener) {
  listeners_.RemoveObserver(listener);
}

void MemoryPressureListenerRegistry::DoNotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  if (base::FeatureList::IsEnabled(kSuppressMemoryListeners)) {
    int start = kSuppressMemoryListenersStart.Get();
    int end = kSuppressMemoryListenersEnd.Get();

    for (auto& listener : listeners_) {
      // Only Notify observers that aren't suppressed. An observer is suppressed
      // if its tag is between `start` (inclusive) and `end` (exclusive)
      if (static_cast<int>(listener.tag()) < start ||
          static_cast<int>(listener.tag()) >= end) {
        listener.Notify(memory_pressure_level);
      }
    }
  } else {
    listeners_.Notify(&SyncMemoryPressureListener::Notify,
                      memory_pressure_level);
  }
}

// static
bool MemoryPressureListenerRegistry::AreNotificationsSuppressed() {
  return g_notifications_suppressed.load(std::memory_order_acquire);
}

// static
void MemoryPressureListenerRegistry::SetNotificationsSuppressed(bool suppress) {
  g_notifications_suppressed.store(suppress, std::memory_order_release);
}

// static
void MemoryPressureListenerRegistry::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  // Notify all listeners even if regular pressure notifications are suppressed.
  Get().DoNotifyMemoryPressure(memory_pressure_level);
}

// static
void MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
    MemoryPressureLevel memory_pressure_level) {
  CHECK(base::SingleThreadTaskRunner::GetMainThreadDefault()
            ->BelongsToCurrentThread());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimulatePressureNotification, memory_pressure_level));
}

}  // namespace base

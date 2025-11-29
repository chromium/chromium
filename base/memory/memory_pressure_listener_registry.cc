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

BASE_FEATURE(kSuppressMemoryListeners, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kSuppressMemoryListenersMask,
                   &kSuppressMemoryListeners,
                   "suppress_memory_listeners_mask",
                   "");
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
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
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

// static
void MemoryPressureListenerRegistry::NotifyMemoryPressureFromAnyThread(
    MemoryPressureLevel memory_pressure_level) {
  auto* main_thread_task_runner =
      SingleThreadTaskRunner::HasMainThreadDefault()
          ? SingleThreadTaskRunner::GetMainThreadDefault().get()
          : nullptr;
  if (!main_thread_task_runner ||
      main_thread_task_runner->BelongsToCurrentThread()) {
    NotifyMemoryPressure(memory_pressure_level);
  } else {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&MemoryPressureListenerRegistry::NotifyMemoryPressure,
                       memory_pressure_level));
  }
}

void MemoryPressureListenerRegistry::AddObserver(
    SyncMemoryPressureListenerRegistration* listener) {
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
  listeners_.AddObserver(listener);
}

void MemoryPressureListenerRegistry::RemoveObserver(
    SyncMemoryPressureListenerRegistration* listener) {
  listeners_.RemoveObserver(listener);
}

void MemoryPressureListenerRegistry::DoNotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
  // Don't repeat MEMORY_PRESSURE_LEVEL_NONE notifications.
  // TODO(464120006): Turn into a CHECK when this can no longer happen.
  if (memory_pressure_level == base::MEMORY_PRESSURE_LEVEL_NONE &&
      last_memory_pressure_level_ == base::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  last_memory_pressure_level_ = memory_pressure_level;
  if (base::FeatureList::IsEnabled(kSuppressMemoryListeners)) {
    auto mask = kSuppressMemoryListenersMask.Get();
    for (auto& listener : listeners_) {
      const size_t tag_index = static_cast<size_t>(listener.tag());
      // Only Notify observers that aren't suppressed. An observer is suppressed
      // if its tag is present in the mask, the value is not '0'. A value of '1'
      // suppresses non critical levels, and a value of '2' supressess all
      // levels.
      if (tag_index >= mask.size() || mask[tag_index] == '0' ||
          (mask[tag_index] == '1' &&
           memory_pressure_level == MEMORY_PRESSURE_LEVEL_CRITICAL)) {
        listener.Notify(memory_pressure_level);
      }
    }
  } else {
    listeners_.Notify(&SyncMemoryPressureListenerRegistration::Notify,
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

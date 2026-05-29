// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener_registry.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/memory/memory_pressure_level.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"

namespace base {

namespace {

MemoryPressureListenerRegistry* g_memory_pressure_listener_registry = nullptr;

BASE_FEATURE(kSuppressMemoryListeners,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(std::string,
                   kSuppressMemoryListenersMask,
                   &kSuppressMemoryListeners,
                   "suppress_memory_listeners_mask",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
                   "0200200202220200020020020002020020000002000000020"
#elif BUILDFLAG(IS_CHROMEOS)
                   "0000000200000200000000000000000000000000000000000"
#elif BUILDFLAG(IS_ANDROID)
                   // Only disable PrerenderHostRegistry.
                   "0000000000000000000000000000000000000000200000000"
#else
                   ""
#endif
);

bool IsListenerSuppressed(MemoryPressureListenerTag tag,
                          MemoryPressureLevel memory_pressure_level,
                          std::string_view mask) {
  const size_t tag_index = static_cast<size_t>(tag);
  if (tag_index >= mask.size()) {
    return false;
  }

  const char mask_char = mask[tag_index];
  switch (mask_char) {
    case '0':
      // '0' means do NOT suppress any notifications (always notify).
      return false;

    case '1':
      // '1' means suppress only MODERATE notifications.
      return memory_pressure_level == MEMORY_PRESSURE_LEVEL_MODERATE;

    case '2':
      // '2' means suppress all notifications.
      return true;

    default:
      // Suppress all on unknown mask characters.
      return true;
  }
}
}  // namespace

// static
bool MemoryPressureListenerRegistry::Exists() {
  return g_memory_pressure_listener_registry;
}

// static
MemoryPressureListenerRegistry& MemoryPressureListenerRegistry::Get() {
  CHECK(g_memory_pressure_listener_registry);
  return *g_memory_pressure_listener_registry;
}

// static
MemoryPressureListenerRegistry* MemoryPressureListenerRegistry::MaybeGet() {
  return g_memory_pressure_listener_registry;
}

MemoryPressureListenerRegistry::MemoryPressureListenerRegistry() {
  CHECK(!g_memory_pressure_listener_registry);
  g_memory_pressure_listener_registry = this;
}

MemoryPressureListenerRegistry::~MemoryPressureListenerRegistry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  listeners_.Notify(&MemoryPressureListenerRegistration::
                        OnBeforeMemoryPressureListenerRegistryDestroyed);
  CHECK(listeners_.empty());

  CHECK_EQ(g_memory_pressure_listener_registry, this);
  g_memory_pressure_listener_registry = nullptr;
}

// static
void MemoryPressureListenerRegistry::NotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());

  if (!Exists()) {
    return;
  }

  Get().SetMemoryPressureLevel(memory_pressure_level);
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
        BindOnce(&MemoryPressureListenerRegistry::NotifyMemoryPressure,
                 memory_pressure_level));
  }
}

void MemoryPressureListenerRegistry::AddObserver(
    MemoryPressureListenerRegistration* listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
  listeners_.AddObserver(listener);

  if (last_memory_pressure_level_ == MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  if (!FeatureList::IsEnabled(kSuppressMemoryListeners) ||
      !IsListenerSuppressed(listener->tag(), last_memory_pressure_level_,
                            kSuppressMemoryListenersMask.Get())) {
    listener->SetInitialMemoryPressureLevel(
        PassKey<MemoryPressureListenerRegistry>(), last_memory_pressure_level_);
  }
}

void MemoryPressureListenerRegistry::RemoveObserver(
    MemoryPressureListenerRegistration* listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  listeners_.RemoveObserver(listener);
}

// static
bool MemoryPressureListenerRegistry::AreNotificationsSuppressed() {
  return Get().AreNotificationsSuppressedImpl();
}

// static
void MemoryPressureListenerRegistry::IncreaseNotificationSuppressionCount() {
  Get().IncreaseNotificationSuppressionCountImpl();
}

// static
void MemoryPressureListenerRegistry::DecreaseNotificationSuppressionCount() {
  Get().DecreaseNotificationSuppressionCountImpl();
}

// static
void MemoryPressureListenerRegistry::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  Get().SimulatePressureNotificationImpl(memory_pressure_level);
}

// static
void MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
    MemoryPressureLevel memory_pressure_level,
    OnceClosure on_notification_sent_callback) {
  CHECK(
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE, BindOnce(&SimulatePressureNotification, memory_pressure_level),
      std::move(on_notification_sent_callback));
}

void MemoryPressureListenerRegistry::SetMemoryPressureLevel(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());

  // Don't repeat MEMORY_PRESSURE_LEVEL_NONE notifications.
  // TODO(464120006): Turn into a CHECK when this can no longer happen.
  if (memory_pressure_level == MEMORY_PRESSURE_LEVEL_NONE &&
      last_memory_pressure_level_ == MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  TRACE_COUNTER("memory", "MemoryPressureLevel",
                static_cast<int>(memory_pressure_level));

  TRACE_EVENT(
      "memory", "MemoryPressureListenerRegistry::SetMemoryPressureLevel",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(
            trace_event::MemoryPressureLevelToTraceEnum(memory_pressure_level));
      });

  last_memory_pressure_level_ = memory_pressure_level;

  // Don't send a notification if they are suppressed.
  if (AreNotificationsSuppressedImpl()) {
    return;
  }

  SendMemoryPressureNotification(last_memory_pressure_level_);
}

void MemoryPressureListenerRegistry::SendMemoryPressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (FeatureList::IsEnabled(kSuppressMemoryListeners)) {
    std::string mask = kSuppressMemoryListenersMask.Get();
    for (auto& listener : listeners_) {
      if (!IsListenerSuppressed(listener.tag(), memory_pressure_level, mask)) {
        listener.UpdateMemoryPressureLevel(
            PassKey<MemoryPressureListenerRegistry>(), memory_pressure_level);
      }
    }
  } else {
    listeners_.Notify(
        &MemoryPressureListenerRegistration::UpdateMemoryPressureLevel,
        PassKey<MemoryPressureListenerRegistry>(), memory_pressure_level);
  }
}

bool MemoryPressureListenerRegistry::AreNotificationsSuppressedImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_suppression_count_ > 0u;
}

void MemoryPressureListenerRegistry::
    IncreaseNotificationSuppressionCountImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++notification_suppression_count_;

  // If notifications suppression was just enabled, remember the current
  // pressure level.
  if (notification_suppression_count_ == 1u) {
    simulated_memory_pressure_level_ = last_memory_pressure_level_;
  }
}

void MemoryPressureListenerRegistry::
    DecreaseNotificationSuppressionCountImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_GT(notification_suppression_count_, 0u);
  --notification_suppression_count_;

  // If notifications suppression was just disabled, clear the simulated level.
  if (notification_suppression_count_ == 0u) {
    if (simulated_memory_pressure_level_.value() !=
        last_memory_pressure_level_) {
      SendMemoryPressureNotification(last_memory_pressure_level_);
    }
    simulated_memory_pressure_level_ = std::nullopt;
  }
}

void MemoryPressureListenerRegistry::SimulatePressureNotificationImpl(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (AreNotificationsSuppressedImpl()) {
    // Notifications are currently suppressed. Use the simulated level to drive
    // notifications.
    if (simulated_memory_pressure_level_ == memory_pressure_level) {
      return;
    }

    simulated_memory_pressure_level_ = memory_pressure_level;
    SendMemoryPressureNotification(memory_pressure_level);
    return;
  }

  // When notifications are not suppressed, this does the same as
  // `NotifyMemoryPressure()`.
  SetMemoryPressureLevel(memory_pressure_level);
}

// MemoryPressureSuppressionToken ----------------------------------------------

MemoryPressureSuppressionToken::MemoryPressureSuppressionToken() {
  MemoryPressureListenerRegistry::IncreaseNotificationSuppressionCount();
}

MemoryPressureSuppressionToken::~MemoryPressureSuppressionToken() {
  MemoryPressureListenerRegistry::DecreaseNotificationSuppressionCount();
}

}  // namespace base

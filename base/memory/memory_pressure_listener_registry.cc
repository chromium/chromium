// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener_registry.h"

#include "base/memory/memory_pressure_level.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"

namespace base {

namespace {

MemoryPressureListenerRegistry* g_memory_pressure_listener_registry = nullptr;

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

  listener->SetInitialMemoryPressureLevel(
      PassKey<MemoryPressureListenerRegistry>(), last_memory_pressure_level_);
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
  // The registry and its listeners live on the process main thread and are
  // guarded by `thread_checker_`. In the single-process model (e.g. Android
  // WebView), the callers of this method (the renderer's MemoryPurgeManager via
  // MemoryPressureSuppressionToken) run on the in-process renderer thread, not
  // the process main thread. Marshal the mutation to the main thread so that
  // all registry state changes happen on its owning thread.
  auto* main_thread_task_runner =
      SingleThreadTaskRunner::HasMainThreadDefault()
          ? SingleThreadTaskRunner::GetMainThreadDefault().get()
          : nullptr;
  if (main_thread_task_runner &&
      !main_thread_task_runner->BelongsToCurrentThread()) {
    main_thread_task_runner->PostTask(
        FROM_HERE, BindOnce(&MemoryPressureListenerRegistry::
                                IncreaseNotificationSuppressionCount));
    return;
  }
  if (auto* registry = MaybeGet()) {
    registry->IncreaseNotificationSuppressionCountImpl();
  }
}

// static
void MemoryPressureListenerRegistry::DecreaseNotificationSuppressionCount() {
  // See IncreaseNotificationSuppressionCount() above. Lifting suppression can
  // synchronously call SendMemoryPressureNotification(), which iterates the
  // listener list and dispatches into browser-thread-owned memory consumers.
  // Running that off the main thread (as happens in the single-process model)
  // races with concurrent registration/notification and can dereference a
  // freed consumer pointer, so marshal to the main thread first.
  auto* main_thread_task_runner =
      SingleThreadTaskRunner::HasMainThreadDefault()
          ? SingleThreadTaskRunner::GetMainThreadDefault().get()
          : nullptr;
  if (main_thread_task_runner &&
      !main_thread_task_runner->BelongsToCurrentThread()) {
    main_thread_task_runner->PostTask(
        FROM_HERE, BindOnce(&MemoryPressureListenerRegistry::
                                DecreaseNotificationSuppressionCount));
    return;
  }
  if (auto* registry = MaybeGet()) {
    registry->DecreaseNotificationSuppressionCountImpl();
  }
}

// static
void MemoryPressureListenerRegistry::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  Get().SimulatePressureNotificationImpl(memory_pressure_level);
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

  TRACE_COUNTER("memory_pressure", "MemoryPressureLevel",
                static_cast<int>(memory_pressure_level));

  TRACE_EVENT(
      "memory_pressure",
      "MemoryPressureListenerRegistry::SetMemoryPressureLevel",
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
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());

  listeners_.Notify(
      &MemoryPressureListenerRegistration::UpdateMemoryPressureLevel,
      PassKey<MemoryPressureListenerRegistry>(), memory_pressure_level);
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

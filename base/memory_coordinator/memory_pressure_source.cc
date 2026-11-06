// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_pressure_source.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"

namespace base {

namespace {

MemoryPressureSource* g_memory_pressure_source = nullptr;

}  // namespace

// static
bool MemoryPressureSource::Exists() {
  return g_memory_pressure_source;
}

// static
MemoryPressureSource& MemoryPressureSource::Get() {
  CHECK(g_memory_pressure_source);
  return *g_memory_pressure_source;
}

// static
MemoryPressureSource* MemoryPressureSource::MaybeGet() {
  return g_memory_pressure_source;
}

MemoryPressureSource::MemoryPressureSource() {
  CHECK(!g_memory_pressure_source);
  g_memory_pressure_source = this;
}

MemoryPressureSource::~MemoryPressureSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_EQ(g_memory_pressure_source, this);
  g_memory_pressure_source = nullptr;
}

// static
void MemoryPressureSource::SetMemoryPressureLevelFromAnyThread(
    MemoryPressureLevel memory_pressure_level) {
  auto* main_thread_task_runner =
      SingleThreadTaskRunner::HasMainThreadDefault()
          ? SingleThreadTaskRunner::GetMainThreadDefault().get()
          : nullptr;
  if (main_thread_task_runner &&
      !main_thread_task_runner->BelongsToCurrentThread()) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        BindOnce(&MemoryPressureSource::SetMemoryPressureLevelFromAnyThread,
                 memory_pressure_level));
    return;
  }
  if (auto* source = MaybeGet()) {
    source->SetMemoryPressureLevel(memory_pressure_level);
  }
}

// static
bool MemoryPressureSource::AreNotificationsSuppressed() {
  return Get().AreNotificationsSuppressedImpl();
}

// static
void MemoryPressureSource::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  Get().SimulatePressureNotificationImpl(memory_pressure_level);
}

CallbackListSubscription MemoryPressureSource::AddCallback(Callback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return callbacks_.Add(std::move(callback));
}

MemoryPressureLevel MemoryPressureSource::GetLastNotifiedMemoryPressureLevel(
    PassKey<MemoryPressureListenerRegistry>) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return simulated_memory_pressure_level_.value_or(last_memory_pressure_level_);
}

// static
void MemoryPressureSource::IncreaseNotificationSuppressionCount() {
  // The source and its subscribers live on the process main thread and are
  // guarded by `thread_checker_`. In the single-process model (e.g. Android
  // WebView), the callers of this method (the renderer's MemoryPurgeManager via
  // MemoryPressureSuppressionToken) run on the in-process renderer thread, not
  // the process main thread. Marshal the mutation to the main thread so that
  // all state changes happen on its owning thread.
  auto* main_thread_task_runner =
      SingleThreadTaskRunner::HasMainThreadDefault()
          ? SingleThreadTaskRunner::GetMainThreadDefault().get()
          : nullptr;
  if (main_thread_task_runner &&
      !main_thread_task_runner->BelongsToCurrentThread()) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        BindOnce(&MemoryPressureSource::IncreaseNotificationSuppressionCount));
    return;
  }
  if (auto* source = MaybeGet()) {
    source->IncreaseNotificationSuppressionCountImpl();
  }
}

// static
void MemoryPressureSource::DecreaseNotificationSuppressionCount() {
  // See IncreaseNotificationSuppressionCount() above. Lifting suppression can
  // synchronously call SendMemoryPressureNotification(), which invokes the
  // callbacks of main-thread-owned subscribers. Running that off the main
  // thread (as happens in the single-process model) races with concurrent
  // subscription/notification and can dereference a freed subscriber pointer,
  // so marshal to the main thread first.
  auto* main_thread_task_runner =
      SingleThreadTaskRunner::HasMainThreadDefault()
          ? SingleThreadTaskRunner::GetMainThreadDefault().get()
          : nullptr;
  if (main_thread_task_runner &&
      !main_thread_task_runner->BelongsToCurrentThread()) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        BindOnce(&MemoryPressureSource::DecreaseNotificationSuppressionCount));
    return;
  }
  if (auto* source = MaybeGet()) {
    source->DecreaseNotificationSuppressionCountImpl();
  }
}

void MemoryPressureSource::SetMemoryPressureLevel(
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
      "memory_pressure", "MemoryPressureSource::SetMemoryPressureLevel",
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

void MemoryPressureSource::SendMemoryPressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(
      !SingleThreadTaskRunner::HasMainThreadDefault() ||
      SingleThreadTaskRunner::GetMainThreadDefault()->BelongsToCurrentThread());

  callbacks_.Notify(memory_pressure_level);
}

bool MemoryPressureSource::AreNotificationsSuppressedImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_suppression_count_ > 0u;
}

void MemoryPressureSource::IncreaseNotificationSuppressionCountImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++notification_suppression_count_;

  // If notifications suppression was just enabled, remember the current
  // pressure level.
  if (notification_suppression_count_ == 1u) {
    simulated_memory_pressure_level_ = last_memory_pressure_level_;
  }
}

void MemoryPressureSource::DecreaseNotificationSuppressionCountImpl() {
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

void MemoryPressureSource::SimulatePressureNotificationImpl(
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
  // `SetMemoryPressureLevel()`.
  SetMemoryPressureLevel(memory_pressure_level);
}

// MemoryPressureSuppressionToken ----------------------------------------------

MemoryPressureSuppressionToken::MemoryPressureSuppressionToken() {
  MemoryPressureSource::IncreaseNotificationSuppressionCount();
}

MemoryPressureSuppressionToken::~MemoryPressureSuppressionToken() {
  MemoryPressureSource::DecreaseNotificationSuppressionCount();
}

}  // namespace base

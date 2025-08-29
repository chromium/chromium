// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"

namespace base {

namespace {

// Controls whether or no MemoryPressureListeners are notified synchronously or,
// in the disabled state, asynchronously. This is only suitable for a listener
// that only lives on the main thread.
BASE_FEATURE(MakeMemoryPressureListenerSync, base::FEATURE_DISABLED_BY_DEFAULT);

std::variant<SyncMemoryPressureListener, AsyncMemoryPressureListener>
CreateMemoryPressureListenerImpl(
    const Location& creation_location,
    MemoryPressureListenerTag tag,
    MemoryPressureListener::MemoryPressureCallback memory_pressure_callback) {
  using ListenerVariant =
      std::variant<SyncMemoryPressureListener, AsyncMemoryPressureListener>;
  if (FeatureList::IsEnabled(kMakeMemoryPressureListenerSync)) {
    return ListenerVariant(std::in_place_type<SyncMemoryPressureListener>, tag,
                           std::move(memory_pressure_callback));
  } else {
    return ListenerVariant(std::in_place_type<AsyncMemoryPressureListener>,
                           creation_location, tag,
                           std::move(memory_pressure_callback));
  }
}

}  // namespace

// SyncMemoryPressureListener --------------------------------------------------

SyncMemoryPressureListener::SyncMemoryPressureListener(
    MemoryPressureListenerTag tag,
    MemoryPressureCallback memory_pressure_callback)
    : memory_pressure_callback_(std::move(memory_pressure_callback)),
      tag_(tag) {
  MemoryPressureListenerRegistry::Get().AddObserver(this);
}

SyncMemoryPressureListener::~SyncMemoryPressureListener() {
  MemoryPressureListenerRegistry::Get().RemoveObserver(this);
}

void SyncMemoryPressureListener::Notify(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  memory_pressure_callback_.Run(memory_pressure_level);
}

// AsyncMemoryPressureListener::MainThread -------------------------------------

class AsyncMemoryPressureListener::MainThread {
 public:
  MainThread() { DETACH_FROM_THREAD(thread_checker_); }

  void Init(WeakPtr<AsyncMemoryPressureListener> parent,
            scoped_refptr<SequencedTaskRunner> listener_task_runner,
            MemoryPressureListenerTag tag) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    listener_task_runner_ = std::move(listener_task_runner);
    parent_ = std::move(parent);
    listener_.emplace(
        tag, BindRepeating(&MainThread::OnMemoryPressure, Unretained(this)));
  }

 private:
  void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    listener_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AsyncMemoryPressureListener::Notify, parent_,
                                  memory_pressure_level));
  }

  // The task runner on which the listener lives.
  scoped_refptr<SequencedTaskRunner> listener_task_runner_
      GUARDED_BY_CONTEXT(thread_checker_);

  // A pointer to the listener that lives on `listener_task_runner_`.
  WeakPtr<AsyncMemoryPressureListener> parent_
      GUARDED_BY_CONTEXT(thread_checker_);

  // The actual sync listener that lives on the main thread.
  std::optional<SyncMemoryPressureListener> listener_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

// AsyncMemoryPressureListener -------------------------------------------------

AsyncMemoryPressureListener::AsyncMemoryPressureListener(
    const base::Location& creation_location,
    MemoryPressureListenerTag tag,
    MemoryPressureCallback memory_pressure_callback)
    : memory_pressure_callback_(std::move(memory_pressure_callback)),
      creation_location_(creation_location) {
  // TODO(crbug.com/40123466): DCHECK instead of silently failing when a
  // MemoryPressureListener is created in a non-sequenced context. Tests will
  // need to be adjusted for that to work.
  if (SingleThreadTaskRunner::HasMainThreadDefault() &&
      SequencedTaskRunner::HasCurrentDefault()) {
    main_thread_task_runner_ = SingleThreadTaskRunner::GetMainThreadDefault();
    main_thread_ = std::make_unique<MainThread>();
    main_thread_task_runner_->PostTask(
        FROM_HERE, BindOnce(&MainThread::Init, Unretained(main_thread_.get()),
                            weak_ptr_factory_.GetWeakPtr(),
                            SequencedTaskRunner::GetCurrentDefault(), tag));
  }
}

AsyncMemoryPressureListener::~AsyncMemoryPressureListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (main_thread_) {
    // To ensure |main_thread_| is deleted on the correct thread, we transfer
    // ownership to a no-op task. The object is deleted with the task, even
    // if it's cancelled before it can run.
    main_thread_task_runner_->PostTask(
        FROM_HERE, BindOnce([](std::unique_ptr<MainThread> main_thread) {},
                            std::move(main_thread_)));
  }
}

void AsyncMemoryPressureListener::Notify(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT(
      "base", "AsyncMemoryPressureListener::Notify",
      [&](perfetto::EventContext ctx) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(
            trace_event::MemoryPressureLevelToTraceEnum(memory_pressure_level));
        data->set_creation_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx,
                                                           creation_location_));
      });
  memory_pressure_callback_.Run(memory_pressure_level);
}

// MemoryPressureListener ------------------------------------------------------

MemoryPressureListener::MemoryPressureListener(
    const Location& creation_location,
    MemoryPressureListenerTag tag,
    MemoryPressureCallback memory_pressure_callback)
    : listener_(CreateMemoryPressureListenerImpl(
          creation_location,
          tag,
          std::move(memory_pressure_callback))) {}

MemoryPressureListener::~MemoryPressureListener() = default;

// static
void MemoryPressureListener::NotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureListenerRegistry::NotifyMemoryPressure(memory_pressure_level);
}

// static
bool MemoryPressureListener::AreNotificationsSuppressed() {
  return MemoryPressureListenerRegistry::AreNotificationsSuppressed();
}

// static
void MemoryPressureListener::SetNotificationsSuppressed(bool suppressed) {
  MemoryPressureListenerRegistry::SetNotificationsSuppressed(suppressed);
}

// static
void MemoryPressureListener::SimulatePressureNotification(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureListenerRegistry::SimulatePressureNotification(
      memory_pressure_level);
}

// static
void MemoryPressureListener::SimulatePressureNotificationAsync(
    MemoryPressureLevel memory_pressure_level) {
  MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
      memory_pressure_level);
}

}  // namespace base

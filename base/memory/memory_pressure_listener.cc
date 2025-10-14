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
BASE_FEATURE(kMakeMemoryPressureListenerSync,
             base::FEATURE_DISABLED_BY_DEFAULT);

std::variant<SyncMemoryPressureListenerRegistration,
             AsyncMemoryPressureListenerRegistration>
CreateMemoryPressureListenerRegistrationImpl(
    const Location& creation_location,
    MemoryPressureListenerTag tag,
    MemoryPressureListener* memory_pressure_listener) {
  using ListenerVariant = std::variant<SyncMemoryPressureListenerRegistration,
                                       AsyncMemoryPressureListenerRegistration>;
  if (FeatureList::IsEnabled(kMakeMemoryPressureListenerSync)) {
    return ListenerVariant(
        std::in_place_type<SyncMemoryPressureListenerRegistration>, tag,
        memory_pressure_listener);
  } else {
    return ListenerVariant(
        std::in_place_type<AsyncMemoryPressureListenerRegistration>,
        creation_location, tag, memory_pressure_listener);
  }
}

}  // namespace

// MemoryPressureListener ------------------------------------------------------

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

// SyncMemoryPressureListenerRegistration --------------------------------------

SyncMemoryPressureListenerRegistration::SyncMemoryPressureListenerRegistration(
    MemoryPressureListenerTag tag,
    MemoryPressureListener* memory_pressure_listener)
    : tag_(tag), memory_pressure_listener_(memory_pressure_listener) {
  MemoryPressureListenerRegistry::Get().AddObserver(this);
}

SyncMemoryPressureListenerRegistration::
    ~SyncMemoryPressureListenerRegistration() {
  MemoryPressureListenerRegistry::Get().RemoveObserver(this);
}

void SyncMemoryPressureListenerRegistration::Notify(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  memory_pressure_listener_->OnMemoryPressure(memory_pressure_level);
}

// AsyncMainThread -------------------------

class AsyncMemoryPressureListenerRegistration::MainThread
    : public MemoryPressureListener {
 public:
  MainThread() { DETACH_FROM_THREAD(thread_checker_); }

  void Init(WeakPtr<AsyncMemoryPressureListenerRegistration> parent,
            scoped_refptr<SequencedTaskRunner> listener_task_runner,
            MemoryPressureListenerTag tag) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    listener_task_runner_ = std::move(listener_task_runner);
    parent_ = std::move(parent);
    listener_.emplace(tag, this);
  }

 private:
  void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    listener_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AsyncMemoryPressureListenerRegistration::Notify,
                       parent_, memory_pressure_level));
  }

  // The task runner on which the listener lives.
  scoped_refptr<SequencedTaskRunner> listener_task_runner_
      GUARDED_BY_CONTEXT(thread_checker_);

  // A pointer to the listener that lives on `listener_task_runner_`.
  WeakPtr<AsyncMemoryPressureListenerRegistration> parent_
      GUARDED_BY_CONTEXT(thread_checker_);

  // The actual sync listener that lives on the main thread.
  std::optional<SyncMemoryPressureListenerRegistration> listener_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

// AsyncMemoryPressureListenerRegistration -------------------------------------

AsyncMemoryPressureListenerRegistration::
    AsyncMemoryPressureListenerRegistration(
        const base::Location& creation_location,
        MemoryPressureListenerTag tag,
        MemoryPressureListener* memory_pressure_listener)
    : memory_pressure_listener_(memory_pressure_listener),
      creation_location_(creation_location) {
  // TODO(crbug.com/40123466): DCHECK instead of silently failing when a
  // MemoryPressureListenerRegistration is created in a non-sequenced context.
  // Tests will need to be adjusted for that to work.
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

AsyncMemoryPressureListenerRegistration::
    ~AsyncMemoryPressureListenerRegistration() {
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

void AsyncMemoryPressureListenerRegistration::Notify(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT(
      "base", "AsyncNotify", [&](perfetto::EventContext ctx) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(
            trace_event::MemoryPressureLevelToTraceEnum(memory_pressure_level));
        data->set_creation_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx,
                                                           creation_location_));
      });
  memory_pressure_listener_->OnMemoryPressure(memory_pressure_level);
}

// MemoryPressureListenerRegistration ------------------------------------------

MemoryPressureListenerRegistration::MemoryPressureListenerRegistration(
    const Location& creation_location,
    MemoryPressureListenerTag tag,
    MemoryPressureListener* memory_pressure_listener)
    : listener_(CreateMemoryPressureListenerRegistrationImpl(
          creation_location,
          tag,
          memory_pressure_listener)) {}

MemoryPressureListenerRegistration::~MemoryPressureListenerRegistration() =
    default;

}  // namespace base

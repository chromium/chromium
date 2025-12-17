// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"

namespace base {

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
    MemoryPressureLevel memory_pressure_level,
    OnceClosure on_notification_sent_callback) {
  MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
      memory_pressure_level, std::move(on_notification_sent_callback));
}

// MemoryPressureListenerRegistration --------------------------------------

MemoryPressureListenerRegistration::MemoryPressureListenerRegistration(
    MemoryPressureListenerTag tag,
    MemoryPressureListener* memory_pressure_listener)
    : tag_(tag),
      memory_pressure_listener_(memory_pressure_listener),
      registry_(MemoryPressureListenerRegistry::MaybeGet()) {
  if (!registry_) {
    DLOG(WARNING) << "Registration of a MemoryPressureListener failed. The "
                     "MemoryPressureListenerRegistry doesn't exist.";
    return;
  }

  registry_->AddObserver(this);
}

MemoryPressureListenerRegistration::MemoryPressureListenerRegistration(
    const Location& creation_location,
    MemoryPressureListenerTag tag,
    MemoryPressureListener* memory_pressure_listener)
    : MemoryPressureListenerRegistration(tag, memory_pressure_listener) {}

MemoryPressureListenerRegistration::~MemoryPressureListenerRegistration() {
  if (!registry_) {
    return;
  }

  CHECK_EQ(registry_, MemoryPressureListenerRegistry::MaybeGet());
  registry_->RemoveObserver(this);
}

void MemoryPressureListenerRegistration::
    OnBeforeMemoryPressureListenerRegistryDestroyed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  registry_->RemoveObserver(this);
  registry_ = nullptr;
}

void MemoryPressureListenerRegistration::Notify(
    MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  memory_pressure_listener_->OnMemoryPressure(memory_pressure_level);
}

// AsyncMemoryPressureListenerRegistration::MainThread -------------------------

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
        FROM_HERE, BindOnce(&AsyncMemoryPressureListenerRegistration::Notify,
                            parent_, memory_pressure_level));
  }

  // The task runner on which the listener lives.
  scoped_refptr<SequencedTaskRunner> listener_task_runner_
      GUARDED_BY_CONTEXT(thread_checker_);

  // A pointer to the listener that lives on `listener_task_runner_`.
  WeakPtr<AsyncMemoryPressureListenerRegistration> parent_
      GUARDED_BY_CONTEXT(thread_checker_);

  // The actual sync listener that lives on the main thread.
  std::optional<MemoryPressureListenerRegistration> listener_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

// AsyncMemoryPressureListenerRegistration -------------------------------------

AsyncMemoryPressureListenerRegistration::
    AsyncMemoryPressureListenerRegistration(
        const Location& creation_location,
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
    // In tests, tasks on the main thread are not executed upon destruction of
    // the TaskEnvironment. The main thread object thus gets tagged as leaking,
    // which is fine in this case.
    ANNOTATE_LEAKING_OBJECT_PTR(main_thread_.get());
    main_thread_task_runner_->DeleteSoon(FROM_HERE, main_thread_.release());
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
            trace_event::InternedSourceLocation::Get(&ctx, creation_location_));
      });
  memory_pressure_listener_->OnMemoryPressure(memory_pressure_level);
}

}  // namespace base

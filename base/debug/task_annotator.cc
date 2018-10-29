// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/task_annotator.h"

#include <array>

#include "base/debug/activity_tracker.h"
#include "base/debug/alias.h"
#include "base/no_destructor.h"
#include "base/pending_task.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace debug {

namespace {

TaskAnnotator::ObserverForTesting* g_task_annotator_observer = nullptr;

// Returns the TLS slot that stores the PendingTask currently in progress on
// each thread. Used to allow creating a breadcrumb of program counters on the
// stack to help identify a task's origin in crashes.
ThreadLocalPointer<const PendingTask>* GetTLSForCurrentPendingTask() {
  static NoDestructor<ThreadLocalPointer<const PendingTask>>
      tls_for_current_pending_task;
  return tls_for_current_pending_task.get();
}

}  // namespace

TaskAnnotator::TaskAnnotator() = default;

TaskAnnotator::~TaskAnnotator() = default;

void TaskAnnotator::WillQueueTask(const char* queue_function,
                                  PendingTask* pending_task) {
  if (queue_function) {
    TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("toplevel.flow"),
                           queue_function,
                           TRACE_ID_MANGLE(GetTaskTraceID(*pending_task)),
                           TRACE_EVENT_FLAG_FLOW_OUT);
  }

  DCHECK(!pending_task->task_backtrace[0])
      << "Task backtrace was already set, task posted twice??";
  if (!pending_task->task_backtrace[0]) {
    const PendingTask* parent_task = GetTLSForCurrentPendingTask()->Get();
    if (parent_task) {
      pending_task->task_backtrace[0] =
          parent_task->posted_from.program_counter();
      std::copy(parent_task->task_backtrace.begin(),
                parent_task->task_backtrace.end() - 1,
                pending_task->task_backtrace.begin() + 1);
    }
  }
}

void TaskAnnotator::RunTask(const char* queue_function,
                            PendingTask* pending_task) {
  ScopedTaskRunActivity task_activity(*pending_task);

  if (queue_function) {
    TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("toplevel.flow"),
                           queue_function,
                           TRACE_ID_MANGLE(GetTaskTraceID(*pending_task)),
                           TRACE_EVENT_FLAG_FLOW_IN);
  }

  // Before running the task, store the task backtrace with the chain of
  // PostTasks that resulted in this call and deliberately alias it to ensure
  // it is on the stack if the task crashes. Be careful not to assume that the
  // variable itself will have the expected value when displayed by the
  // optimizer in an optimized build. Look at a memory dump of the stack.
  static constexpr int kStackTaskTraceSnapshotSize =
      std::tuple_size<decltype(pending_task->task_backtrace)>::value + 3;
  std::array<const void*, kStackTaskTraceSnapshotSize> task_backtrace;

  // Store a marker to locate |task_backtrace| content easily on a memory
  // dump.
  task_backtrace.front() = reinterpret_cast<void*>(0xefefefefefefefef);
  task_backtrace.back() = reinterpret_cast<void*>(0xfefefefefefefefe);

  task_backtrace[1] = pending_task->posted_from.program_counter();
  std::copy(pending_task->task_backtrace.begin(),
            pending_task->task_backtrace.end(), task_backtrace.begin() + 2);
  debug::Alias(&task_backtrace);

  ThreadLocalPointer<const PendingTask>* tls_for_current_pending_task =
      GetTLSForCurrentPendingTask();
  const PendingTask* previous_pending_task =
      tls_for_current_pending_task->Get();
  tls_for_current_pending_task->Set(pending_task);

  if (g_task_annotator_observer)
    g_task_annotator_observer->BeforeRunTask(pending_task);
  std::move(pending_task->task).Run();

  tls_for_current_pending_task->Set(previous_pending_task);
}

uint64_t TaskAnnotator::GetTaskTraceID(const PendingTask& task) const {
  return (static_cast<uint64_t>(task.sequence_num) << 32) |
         ((static_cast<uint64_t>(reinterpret_cast<intptr_t>(this)) << 32) >>
          32);
}

// static
void TaskAnnotator::RegisterObserverForTesting(ObserverForTesting* observer) {
  DCHECK(!g_task_annotator_observer);
  g_task_annotator_observer = observer;
}

// static
void TaskAnnotator::ClearObserverForTesting() {
  g_task_annotator_observer = nullptr;
}

}  // namespace debug
}  // namespace base

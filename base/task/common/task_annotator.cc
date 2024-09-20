// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/task_annotator.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <string_view>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_mojo_event_info.pbzero.h"  // nogncheck
#endif

namespace base {

namespace {

TaskAnnotator::ObserverForTesting* g_task_annotator_observer = nullptr;

// The PendingTask currently in progress on each thread. Used to allow creating
// a breadcrumb of program counters on the stack to help identify a task's
// origin in crashes.
constinit thread_local PendingTask* current_pending_task = nullptr;

// Scoped IPC-related data (IPC hash and/or IPC interface name). IPC hash or
// interface name can be known before the associated task object is created;
// thread-local so that this data can be affixed to the associated task.
constinit thread_local TaskAnnotator::ScopedSetIpcHash*
    current_scoped_ipc_hash = nullptr;

constinit thread_local TaskAnnotator::LongTaskTracker*
    current_long_task_tracker = nullptr;

// These functions can be removed, and the calls below replaced with direct
// variable accesses, once the MSAN workaround is not necessary.
TaskAnnotator::ScopedSetIpcHash* GetCurrentScopedIpcHash() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&current_scoped_ipc_hash,
                sizeof(TaskAnnotator::ScopedSetIpcHash*));

  return current_scoped_ipc_hash;
}

TaskAnnotator::LongTaskTracker* GetCurrentLongTaskTracker() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&current_long_task_tracker,
                sizeof(TaskAnnotator::LongTaskTracker*));

  return current_long_task_tracker;
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
perfetto::protos::pbzero::ChromeTaskAnnotator::DelayPolicy ToProtoEnum(
    subtle::DelayPolicy type) {
  using ProtoType = perfetto::protos::pbzero::ChromeTaskAnnotator::DelayPolicy;
  switch (type) {
    case subtle::DelayPolicy::kFlexibleNoSooner:
      return ProtoType::FLEXIBLE_NO_SOONER;
    case subtle::DelayPolicy::kFlexiblePreferEarly:
      return ProtoType::FLEXIBLE_PREFER_EARLY;
    case subtle::DelayPolicy::kPrecise:
      return ProtoType::PRECISE;
  }
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

}  // namespace

const PendingTask* TaskAnnotator::CurrentTaskForThread() {
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&current_pending_task, sizeof(PendingTask*));

  return current_pending_task;
}

void TaskAnnotator::OnIPCReceived(const char* interface_name,
                                  uint32_t (*method_info)(),
                                  bool is_response) {
  auto* const tracker = GetCurrentLongTaskTracker();
  if (!tracker) {
    return;
  }

  tracker->SetIpcDetails(interface_name, method_info, is_response);
}

void TaskAnnotator::MarkCurrentTaskAsInterestingForTracing() {
  auto* const tracker = GetCurrentLongTaskTracker();
  if (!tracker) {
    return;
  }

  tracker->is_interesting_task = true;
}

TaskAnnotator::TaskAnnotator() = default;
TaskAnnotator::~TaskAnnotator() = default;

void TaskAnnotator::WillQueueTask(perfetto::StaticString trace_event_name,
                                  TaskMetadata* pending_task) {
  DCHECK(pending_task);
  TRACE_EVENT_INSTANT(
      "toplevel.flow", trace_event_name,
      perfetto::Flow::ProcessScoped(GetTaskTraceID(*pending_task)));

  DCHECK(!pending_task->task_backtrace[0])
      << "Task backtrace was already set, task posted twice??";
  if (pending_task->task_backtrace[0])
    return;

  DCHECK(!pending_task->ipc_interface_name);
  DCHECK(!pending_task->ipc_hash);
  const auto* const hash = GetCurrentScopedIpcHash();
  if (hash) {
    pending_task->ipc_interface_name = hash->GetIpcInterfaceName();
    pending_task->ipc_hash = hash->GetIpcHash();
  }

  const auto* parent_task = CurrentTaskForThread();
  if (!parent_task)
    return;

  pending_task->task_backtrace[0] = parent_task->posted_from.program_counter();
  std::copy(parent_task->task_backtrace.begin(),
            parent_task->task_backtrace.end() - 1,
            pending_task->task_backtrace.begin() + 1);
  pending_task->task_backtrace_overflow =
      parent_task->task_backtrace_overflow ||
      parent_task->task_backtrace.back() != nullptr;
}

void TaskAnnotator::RunTaskImpl(PendingTask& pending_task) {
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION(
      pending_task.posted_from.file_name());

  // Before running the task, store the IPC context and the task backtrace with
  // the chain of PostTasks that resulted in this call and deliberately alias it
  // to ensure it is on the stack if the task crashes. Be careful not to assume
  // that the variable itself will have the expected value when displayed by the
  // optimizer in an optimized build. Look at a memory dump of the stack.
  static constexpr int kStackTaskTraceSnapshotSize =
      PendingTask::kTaskBacktraceLength + 4;
  std::array<const void*, kStackTaskTraceSnapshotSize> task_backtrace;

  // Store a marker to locate |task_backtrace| content easily on a memory
  // dump. The layout is as follows:
  //
  // +------------ +----+---------+-----+-----------+----------+-------------+
  // | Head Marker | PC | frame 0 | ... | frame N-1 | IPC hash | Tail Marker |
  // +------------ +----+---------+-----+-----------+----------+-------------+
  //
  // Markers glossary (compliments of wez):
  //      cool code,do it dude!
  //   0x c001 c0de d0 17 d00d
  //      o dude,i did it biig
  //   0x 0 d00d 1 d1d 17 8119
  task_backtrace.front() = reinterpret_cast<void*>(0xc001c0ded017d00d);
  task_backtrace.back() = reinterpret_cast<void*>(0x0d00d1d1d178119);

  task_backtrace[1] = pending_task.posted_from.program_counter();
  ranges::copy(pending_task.task_backtrace, task_backtrace.begin() + 2);
  task_backtrace[kStackTaskTraceSnapshotSize - 2] =
      reinterpret_cast<void*>(pending_task.ipc_hash);
  debug::Alias(&task_backtrace);

  // Record the task time in convenient units. This can be compared to times
  // stored in places like ReportThreadHang() and BrowserMain() when analyzing
  // hangs.
  const int64_t task_time =
      pending_task.GetDesiredExecutionTime().since_origin().InSeconds();
  base::debug::Alias(&task_time);

  {
    const AutoReset<PendingTask*> resetter(&current_pending_task,
                                           &pending_task);

    if (g_task_annotator_observer) {
      g_task_annotator_observer->BeforeRunTask(&pending_task);
    }
    std::move(pending_task.task).Run();
  }

  // Stomp the markers. Otherwise they can stick around on the unused parts of
  // stack and cause |task_backtrace| to be associated with an unrelated stack
  // sample on this thread later in the event of a crash. Alias once again after
  // these writes to make sure the compiler doesn't optimize them out (unused
  // writes to a local variable).
  task_backtrace.front() = nullptr;
  task_backtrace.back() = nullptr;
  debug::Alias(&task_backtrace);
}

uint64_t TaskAnnotator::GetTaskTraceID(const TaskMetadata& task) const {
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

#if BUILDFLAG(ENABLE_BASE_TRACING)
// TRACE_EVENT argument helper, writing the task location data into
// EventContext.
void TaskAnnotator::EmitTaskLocation(perfetto::EventContext& ctx,
                                     const PendingTask& task) {
  ctx.event()->set_task_execution()->set_posted_from_iid(
      base::trace_event::InternedSourceLocation::Get(&ctx, task.posted_from));
}

// TRACE_EVENT argument helper, writing the incoming task flow information
// into EventContext if toplevel.flow category is enabled.
void TaskAnnotator::MaybeEmitIncomingTaskFlow(perfetto::EventContext& ctx,
                                              const PendingTask& task) const {
  static const uint8_t* flow_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("toplevel.flow");
  if (!*flow_enabled)
    return;

  perfetto::Flow::ProcessScoped(GetTaskTraceID(task))(ctx);
}

// static
void TaskAnnotator::MaybeEmitDelayAndPolicy(perfetto::EventContext& ctx,
                                            const PendingTask& task) {
  if (task.delayed_run_time.is_null()) {
    return;
  }
  auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* annotator = event->set_chrome_task_annotator();
  annotator->set_task_delay_us(static_cast<uint64_t>(
      (task.delayed_run_time - task.queue_time).InMicroseconds()));
  annotator->set_delay_policy(ToProtoEnum(task.delay_policy));
}

void TaskAnnotator::MaybeEmitIPCHash(perfetto::EventContext& ctx,
                                     const PendingTask& task) const {
  static const uint8_t* toplevel_ipc_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("toplevel.ipc"));
  if (!*toplevel_ipc_enabled)
    return;

  auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* annotator = event->set_chrome_task_annotator();
  annotator->set_ipc_hash(task.ipc_hash);
}
#endif  //  BUILDFLAG(ENABLE_BASE_TRACING)

TaskAnnotator::ScopedSetIpcHash::ScopedSetIpcHash(uint32_t ipc_hash)
    : ScopedSetIpcHash(ipc_hash, nullptr) {}

TaskAnnotator::ScopedSetIpcHash::ScopedSetIpcHash(
    const char* ipc_interface_name)
    : ScopedSetIpcHash(0, ipc_interface_name) {}

TaskAnnotator::ScopedSetIpcHash::ScopedSetIpcHash(
    uint32_t ipc_hash,
    const char* ipc_interface_name)
    : resetter_(&current_scoped_ipc_hash, this),
      ipc_hash_(ipc_hash),
      ipc_interface_name_(ipc_interface_name) {}

// Static
uint32_t TaskAnnotator::ScopedSetIpcHash::MD5HashMetricName(
    std::string_view name) {
  return HashMetricNameAs32Bits(name);
}

TaskAnnotator::ScopedSetIpcHash::~ScopedSetIpcHash() {
  DCHECK_EQ(this, GetCurrentScopedIpcHash());
}

TaskAnnotator::LongTaskTracker::LongTaskTracker(const TickClock* tick_clock,
                                                PendingTask& pending_task,
                                                TaskAnnotator* task_annotator)
    : resetter_(&current_long_task_tracker, this),
      tick_clock_(tick_clock),
      pending_task_(pending_task),
      task_annotator_(task_annotator) {
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("scheduler.long_tasks", &is_tracing_);
  if (is_tracing_) {
    task_start_time_ = tick_clock_->NowTicks();
  }
}

TaskAnnotator::LongTaskTracker::~LongTaskTracker() {
  DCHECK_EQ(this, GetCurrentLongTaskTracker());

  if (!is_tracing_)
    return;

  task_end_time_ = tick_clock_->NowTicks();
  MaybeTraceInterestingTaskDetails();

  if ((task_end_time_ - task_start_time_) >= kMaxTaskDurationTimeDelta) {
    TRACE_EVENT_BEGIN("scheduler.long_tasks", "LongTaskTracker",
                      perfetto::Track::ThreadScoped(task_annotator_),
                      task_start_time_, [&](perfetto::EventContext& ctx) {
                        TaskAnnotator::EmitTaskLocation(ctx, pending_task_);
                        EmitReceivedIPCDetails(ctx);
                      });
    TRACE_EVENT_END("scheduler.long_tasks",
                    perfetto::Track::ThreadScoped(task_annotator_),
                    task_end_time_);
  }
}

void TaskAnnotator::LongTaskTracker::SetIpcDetails(const char* interface_name,
                                                   uint32_t (*method_info)(),
                                                   bool is_response) {
  ipc_interface_name_ = interface_name;
  is_response_ = is_response;

  if (!method_info)
    return;

  ipc_hash_ = (*method_info)();
  ipc_method_info_ = method_info;
}

void TaskAnnotator::LongTaskTracker::EmitReceivedIPCDetails(
    perfetto::EventContext& ctx) {
  if (!ipc_interface_name_ || !ipc_hash_ || !ipc_method_info_)
    return;
#if BUILDFLAG(ENABLE_BASE_TRACING) && !BUILDFLAG(IS_NACL)
  // Emit all of the IPC hash information if this task
  // comes from a mojo interface.
  auto* info = ctx.event()->set_chrome_mojo_event_info();
  info->set_mojo_interface_tag(ipc_interface_name_);
  info->set_ipc_hash(ipc_hash_);
  info->set_is_reply(is_response_);

  // The Native client will not build as the relevant implementation of
  // base::ModuleCache::CreateModuleForAddress is not implemented for it.
  // Thus the below code must be included on a conditional basis.
  const auto ipc_method_address = reinterpret_cast<uintptr_t>(ipc_method_info_);
  const std::optional<size_t> location_iid =
      base::trace_event::InternedUnsymbolizedSourceLocation::Get(
          &ctx, ipc_method_address);
  if (location_iid) {
    info->set_mojo_interface_method_iid(*location_iid);
  }
#endif
}

// This method is used to record the queueing time and task start time for tasks
// that may be of interest during a trace, even if they are not considered long
// tasks. For example, input - the queue time and flow information is required
// to calculate chrome input to browser intervals in perfetto, and further
// calculate the chrome tasks blocking input. We need LatencyInfo slices to be
// associated with the correct input IPCs, hence record in the LongTaskTracker.
void TaskAnnotator::LongTaskTracker::MaybeTraceInterestingTaskDetails() {
  if (is_interesting_task && ipc_interface_name_) {
    // Record the equivalent of a delayed instant trace event, acting as the
    // start of the flow between task queue time and task execution start time.
    TRACE_EVENT_INSTANT("scheduler.long_tasks", "InterestingTask_QueueingTime",
                        perfetto::Track::ThreadScoped(task_annotator_),
                        pending_task_.queue_time,
                        perfetto::Flow::ProcessScoped(
                            task_annotator_->GetTaskTraceID(pending_task_)));

    // Record the equivalent of a top-level event with enough IPC information
    // to calculate the input to browser interval. This event will be the
    // termination of the event above, aka the start of task execution.
    TRACE_EVENT_BEGIN(
        "scheduler.long_tasks", "InterestingTask_ProcessingTime",
        perfetto::Track::ThreadScoped(task_annotator_), task_start_time_,
        [&](perfetto::EventContext& ctx) {
          perfetto::TerminatingFlow::ProcessScoped(
              task_annotator_->GetTaskTraceID(pending_task_))(ctx);
          auto* info = ctx.event()->set_chrome_mojo_event_info();
          info->set_mojo_interface_tag(ipc_interface_name_);
        });

    TRACE_EVENT_END("scheduler.long_tasks",
                    perfetto::Track::ThreadScoped(task_annotator_),
                    task_end_time_);
  }
}

}  // namespace base

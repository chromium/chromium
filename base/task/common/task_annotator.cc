// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/task_annotator.h"

#include <stdint.h>
#include <algorithm>
#include <array>

#include "base/check_op.h"
#include "base/debug/activity_tracker.h"
#include "base/debug/alias.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_mojo_event_info.pbzero.h"  // nogncheck
#endif

namespace base {

namespace {

TaskAnnotator::ObserverForTesting* g_task_annotator_observer = nullptr;

// Returns the TLS slot that stores the PendingTask currently in progress on
// each thread. Used to allow creating a breadcrumb of program counters on the
// stack to help identify a task's origin in crashes.
ThreadLocalPointer<PendingTask>* GetTLSForCurrentPendingTask() {
  static NoDestructor<ThreadLocalPointer<PendingTask>> instance;
  return instance.get();
}

// Returns the TLS slot that stores scoped IPC-related data (IPC hash and/or
// IPC interface name). IPC hash or interface name can be known before the
// associated task object is created; store in the TLS so that this data can be
// affixed to the associated task.
ThreadLocalPointer<TaskAnnotator::ScopedSetIpcHash>*
GetTLSForCurrentScopedIpcHash() {
  static NoDestructor<ThreadLocalPointer<TaskAnnotator::ScopedSetIpcHash>>
      instance;
  return instance.get();
}

ThreadLocalPointer<TaskAnnotator::LongTaskTracker>*
GetTLSForCurrentLongTaskTracker() {
  static NoDestructor<ThreadLocalPointer<TaskAnnotator::LongTaskTracker>>
      instance;
  return instance.get();
}

}  // namespace

const PendingTask* TaskAnnotator::CurrentTaskForThread() {
  return GetTLSForCurrentPendingTask()->Get();
}

void TaskAnnotator::OnIPCReceived(const char* interface_name,
                                  uint32_t (*method_info)(),
                                  bool is_response) {
  base::TaskAnnotator::LongTaskTracker* current_long_task_tracker =
      GetTLSForCurrentLongTaskTracker()->Get();

  if (!current_long_task_tracker)
    return;

  current_long_task_tracker->SetIpcDetails(interface_name, method_info,
                                           is_response);
}

TaskAnnotator::TaskAnnotator() = default;
TaskAnnotator::~TaskAnnotator() = default;

void TaskAnnotator::WillQueueTask(perfetto::StaticString trace_event_name,
                                  PendingTask* pending_task) {
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
  auto* current_ipc_hash = GetTLSForCurrentScopedIpcHash()->Get();
  if (current_ipc_hash) {
    pending_task->ipc_interface_name = current_ipc_hash->GetIpcInterfaceName();
    pending_task->ipc_hash = current_ipc_hash->GetIpcHash();
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
  debug::ScopedTaskRunActivity task_activity(pending_task);

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

  auto* tls = GetTLSForCurrentPendingTask();
  auto* previous_pending_task = tls->Get();
  tls->Set(&pending_task);

  if (g_task_annotator_observer)
    g_task_annotator_observer->BeforeRunTask(&pending_task);
  std::move(pending_task.task).Run();
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_FAMILY)
  // Some tasks on some machines clobber the non-volatile XMM registers in
  // violation of the Windows ABI. This empty assembly language block with
  // clobber directives tells the compiler to assume that these registers
  // may have lost their values. This ensures that this function will not rely
  // on the registers retaining their values, and it ensures that it will
  // restore the values when this function ends. This is needed because the
  // code-gen for at least one caller of this function in official builds relies
  // on an XMM register (usually XMM7, cleared to zero) maintaining its value as
  // multiple tasks are run, which causes crashes if it is corrupted, since
  // "zeroed" variables end up not being zeroed.
  // The third-party issue is believed to be fixed but will take a while to
  // propagate to users which is why this mitigation is needed.
  // For details see https://crbug.com/1218384
  asm(""
      :
      :
      : "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12",
        "%xmm13", "%xmm14", "%xmm15");
#endif

  tls->Set(previous_pending_task);

  // Stomp the markers. Otherwise they can stick around on the unused parts of
  // stack and cause |task_backtrace| to be associated with an unrelated stack
  // sample on this thread later in the event of a crash. Alias once again after
  // these writes to make sure the compiler doesn't optimize them out (unused
  // writes to a local variable).
  task_backtrace.front() = nullptr;
  task_backtrace.back() = nullptr;
  debug::Alias(&task_backtrace);
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

  perfetto::TerminatingFlow::ProcessScoped(GetTaskTraceID(task))(ctx);
}

void TaskAnnotator::MaybeEmitIPCHashAndDelay(perfetto::EventContext& ctx,
                                             const PendingTask& task) const {
  static const uint8_t* toplevel_ipc_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("toplevel.ipc"));
  if (!*toplevel_ipc_enabled)
    return;

  auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* annotator = event->set_chrome_task_annotator();
  annotator->set_ipc_hash(task.ipc_hash);
  if (!task.delayed_run_time.is_null()) {
    annotator->set_task_delay_us(static_cast<uint64_t>(
        (task.delayed_run_time - task.queue_time).InMicroseconds()));
  }
}
#endif  //  BUILDFLAG(ENABLE_BASE_TRACING)

TaskAnnotator::ScopedSetIpcHash::ScopedSetIpcHash(uint32_t ipc_hash)
    : ScopedSetIpcHash(ipc_hash, nullptr) {}

TaskAnnotator::ScopedSetIpcHash::ScopedSetIpcHash(
    const char* ipc_interface_name)
    : ScopedSetIpcHash(0, ipc_interface_name) {}

TaskAnnotator::ScopedSetIpcHash::ScopedSetIpcHash(
    uint32_t ipc_hash,
    const char* ipc_interface_name) {
  auto* tls_ipc_hash = GetTLSForCurrentScopedIpcHash();
  auto* current_ipc_hash = tls_ipc_hash->Get();
  old_scoped_ipc_hash_ = current_ipc_hash;
  ipc_hash_ = ipc_hash;
  ipc_interface_name_ = ipc_interface_name;
  tls_ipc_hash->Set(this);
}

// Static
uint32_t TaskAnnotator::ScopedSetIpcHash::MD5HashMetricName(
    base::StringPiece name) {
  base::MD5Digest digest;
  base::MD5Sum(name.data(), name.size(), &digest);
  uint32_t value;
  DCHECK_GE(sizeof(digest.a), sizeof(value));
  memcpy(&value, digest.a, sizeof(value));
  return base::NetToHost32(value);
}

TaskAnnotator::ScopedSetIpcHash::~ScopedSetIpcHash() {
  auto* tls_ipc_hash = GetTLSForCurrentScopedIpcHash();
  DCHECK_EQ(this, tls_ipc_hash->Get());
  tls_ipc_hash->Set(old_scoped_ipc_hash_.get());
}

TaskAnnotator::LongTaskTracker::LongTaskTracker(const TickClock* tick_clock,
                                                PendingTask& pending_task,
                                                TaskAnnotator* task_annotator)
    : tick_clock_(tick_clock),
      pending_task_(pending_task),
      task_annotator_(task_annotator) {
  auto* tls_long_task_tracker = GetTLSForCurrentLongTaskTracker();
  old_long_task_tracker_ = tls_long_task_tracker->Get();

  TRACE_EVENT_CATEGORY_GROUP_ENABLED("scheduler.long_tasks", &is_tracing_);
  if (is_tracing_) {
    task_start_time_ = tick_clock_->NowTicks();
  }

  tls_long_task_tracker->Set(this);
}

TaskAnnotator::LongTaskTracker::~LongTaskTracker() {
  auto* tls_long_task_tracker = GetTLSForCurrentLongTaskTracker();
  DCHECK_EQ(this, tls_long_task_tracker->Get());
  tls_long_task_tracker->Set(old_long_task_tracker_.get());

  if (!is_tracing_)
    return;

  TimeTicks task_end_time = tick_clock_->NowTicks();
  if ((task_end_time - task_start_time_) >= kMaxTaskDurationTimeDelta) {
    TRACE_EVENT_BEGIN("scheduler.long_tasks", "LongTaskTracker",
                      perfetto::Track::ThreadScoped(task_annotator_),
                      task_start_time_, [&](perfetto::EventContext& ctx) {
                        TaskAnnotator::EmitTaskLocation(ctx, pending_task_);
                        EmitReceivedIPCDetails(ctx);
                      });
    TRACE_EVENT_END("scheduler.long_tasks",
                    perfetto::Track::ThreadScoped(task_annotator_),
                    task_end_time);
  }
#if !BUILDFLAG(ENABLE_BASE_TRACING)
  // Suppress the unused variable warning when TRACE_EVENT macros are turned
  // into no-op.
  (void)pending_task_;
  (void)task_annotator_;
#endif  // !BUILDFLAG(ENABLE_BASE_TRACING)
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
  const absl::optional<size_t> location_iid =
      base::trace_event::InternedUnsymbolizedSourceLocation::Get(
          &ctx, ipc_method_address);
  if (location_iid) {
    info->set_mojo_interface_method_iid(*location_iid);
  }
#endif
}

}  // namespace base

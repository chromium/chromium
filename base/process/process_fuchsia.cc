// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <lib/zx/process.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "base/clang_profiling_buildflags.h"
#include "base/debug/activity_tracker.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(CLANG_PROFILING)
#include "base/test/clang_profiling.h"
#endif

namespace base {

namespace {

zx::process FindProcessInJobTree(const zx::job& job, ProcessId pid) {
  zx::process process;
  zx_status_t status = job.get_child(pid, ZX_RIGHT_SAME_RIGHTS, &process);

  if (status == ZX_OK)
    return process;

  if (status == ZX_ERR_NOT_FOUND) {
    std::vector<zx_koid_t> job_koids(32);
    while (true) {
      // Fetch the KOIDs of the job children of |job|.
      size_t actual = 0u;
      size_t available = 0u;
      status = job.get_info(ZX_INFO_JOB_CHILDREN, job_koids.data(),
                            job_koids.size() * sizeof(zx_koid_t), &actual,
                            &available);

      if (status != ZX_OK) {
        ZX_DLOG(ERROR, status) << "zx_object_get_info(JOB_CHILDREN)";
        return zx::process();
      }

      // If |job_koids| was too small then resize it and try again.
      if (available > actual) {
        job_koids.resize(available);
        continue;
      }

      // Break out of the loop and iterate over |job_koids|, to find the PID.
      job_koids.resize(actual);
      break;
    }

    for (zx_koid_t job_koid : job_koids) {
      zx::job child_job;
      if (job.get_child(job_koid, ZX_RIGHT_SAME_RIGHTS, &child_job) != ZX_OK)
        continue;
      process = FindProcessInJobTree(child_job, pid);
      if (process)
        return process;
    }

    return zx::process();
  }

  ZX_DLOG(ERROR, status) << "zx_object_get_child";
  return zx::process();
}

}  // namespace

Process::Process(ProcessHandle handle)
    : process_(handle), is_current_process_(false) {
  CHECK_NE(handle, zx_process_self());
}

Process::~Process() {
  Close();
}

Process::Process(Process&& other)
    : process_(std::move(other.process_)),
      is_current_process_(other.is_current_process_) {
  other.is_current_process_ = false;
}

Process& Process::operator=(Process&& other) {
  process_ = std::move(other.process_);
  is_current_process_ = other.is_current_process_;
  other.is_current_process_ = false;
  return *this;
}

// static
Process Process::Current() {
  Process process;
  process.is_current_process_ = true;
  return process;
}

// static
Process Process::Open(ProcessId pid) {
  if (pid == GetCurrentProcId())
    return Current();

  return Process(FindProcessInJobTree(*GetDefaultJob(), pid).release());
}

// static
Process Process::OpenWithExtraPrivileges(ProcessId pid) {
  // No privileges to set.
  return Open(pid);
}

// static
bool Process::CanBackgroundProcesses() {
  return false;
}

// static
void Process::TerminateCurrentProcessImmediately(int exit_code) {
#if BUILDFLAG(CLANG_PROFILING)
  WriteClangProfilingProfile();
#endif
  _exit(exit_code);
}

bool Process::IsValid() const {
  return process_.is_valid() || is_current();
}

ProcessHandle Process::Handle() const {
  return is_current_process_ ? zx_process_self() : process_.get();
}

Process Process::Duplicate() const {
  if (is_current())
    return Current();

  if (!IsValid())
    return Process();

  zx::process out;
  zx_status_t result = process_.duplicate(ZX_RIGHT_SAME_RIGHTS, &out);
  if (result != ZX_OK) {
    ZX_DLOG(ERROR, result) << "zx_handle_duplicate";
    return Process();
  }

  return Process(out.release());
}

ProcessHandle Process::Release() {
  if (is_current()) {
    // Caller expects to own the reference, so duplicate the self handle.
    zx::process handle;
    zx_status_t result =
        zx::process::self()->duplicate(ZX_RIGHT_SAME_RIGHTS, &handle);
    if (result != ZX_OK) {
      return kNullProcessHandle;
    }
    is_current_process_ = false;
    return handle.release();
  }
  return process_.release();
}

ProcessId Process::Pid() const {
  DCHECK(IsValid());
  return GetProcId(Handle());
}

Time Process::CreationTime() const {
  zx_info_process_t proc_info;
  zx_status_t status =
      zx_object_get_info(Handle(), ZX_INFO_PROCESS, &proc_info,
                         sizeof(proc_info), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_process_get_info";
    return Time();
  }
  if ((proc_info.flags & ZX_INFO_PROCESS_FLAG_STARTED) == 0) {
    DLOG(WARNING) << "zx_process_get_info: Not started.";
    return Time();
  }
  // Process creation times are expressed in ticks since system boot, so
  // perform a best-effort translation from that to UTC "wall-clock" time.
  return Time::Now() +
         (TimeTicks::FromZxTime(proc_info.start_time) - TimeTicks::Now());
}

bool Process::is_current() const {
  return is_current_process_;
}

void Process::Close() {
  is_current_process_ = false;
  process_.reset();
}

bool Process::Terminate(int exit_code, bool wait) const {
  // exit_code isn't supportable. https://crbug.com/753490.
  zx_status_t status = zx_task_kill(Handle());
  if (status == ZX_OK && wait) {
    zx_signals_t signals;
    status = zx_object_wait_one(Handle(), ZX_TASK_TERMINATED,
                                zx_deadline_after(ZX_SEC(60)), &signals);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_object_wait_one(terminate)";
    } else {
      CHECK(signals & ZX_TASK_TERMINATED);
    }
  } else if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_task_kill";
  }

  return status >= 0;
}

bool Process::WaitForExit(int* exit_code) const {
  return WaitForExitWithTimeout(TimeDelta::Max(), exit_code);
}

bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const {
  if (is_current_process_)
    return false;

  TRACE_EVENT0("base", "Process::WaitForExitWithTimeout");

  // Record the event that this thread is blocking upon (for hang diagnosis).
  absl::optional<debug::ScopedProcessWaitActivity> process_activity;
  if (!timeout.is_zero()) {
    process_activity.emplace(this);
    // Assert that this thread is allowed to wait below. This intentionally
    // doesn't use ScopedBlockingCallWithBaseSyncPrimitives because the process
    // being waited upon tends to itself be using the CPU and considering this
    // thread non-busy causes more issue than it fixes: http://crbug.com/905788
    internal::AssertBaseSyncPrimitivesAllowed();
  }

  zx::time deadline = timeout == TimeDelta::Max()
                          ? zx::time::infinite()
                          : zx::time((TimeTicks::Now() + timeout).ToZxTime());
  zx_signals_t signals_observed = 0;
  zx_status_t status =
      process_.wait_one(ZX_TASK_TERMINATED, deadline, &signals_observed);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_object_wait_one";
    return false;
  }

  zx_info_process_v2_t proc_info;
  status = process_.get_info(ZX_INFO_PROCESS_V2, &proc_info, sizeof(proc_info),
                             nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_object_get_info";
    if (exit_code)
      *exit_code = -1;
    return false;
  }

  if (exit_code)
    *exit_code = proc_info.return_code;

  return true;
}

void Process::Exited(int exit_code) const {}

bool Process::IsProcessBackgrounded() const {
  // See SetProcessBackgrounded().
  DCHECK(IsValid());
  return false;
}

bool Process::SetProcessBackgrounded(bool value) {
  // No process priorities on Fuchsia.
  // TODO(fxbug.dev/30735): Update this later if priorities are implemented.
  return false;
}

int Process::GetPriority() const {
  DCHECK(IsValid());
  // No process priorities on Fuchsia.
  return 0;
}

}  // namespace base

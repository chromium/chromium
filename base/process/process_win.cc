// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <windows.h>

#include "base/clang_profiling_buildflags.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/base_tracing.h"
#include "base/win/windows_version.h"

#if BUILDFLAG(CLANG_PROFILING)
#include "base/test/clang_profiling.h"
#endif

namespace {

DWORD kBasicProcessAccess =
  PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE;

} // namespace

namespace base {

// Sets Eco QoS (Quality of Service) level for background process which would
// select efficient CPU frequency and schedule the process to efficient cores
// (available on hybrid CPUs).
// QoS is a scheduling Win API which indicates the desired performance and power
// efficiency of a process/thread. EcoQoS is introduced since Windows 11.
BASE_FEATURE(kUseEcoQoSForBackgroundProcess,
             "UseEcoQoSForBackgroundProcess",
             FEATURE_ENABLED_BY_DEFAULT);

Process::Process(ProcessHandle handle)
    : process_(handle), is_current_process_(false) {
  CHECK_NE(handle, ::GetCurrentProcess());
}

Process::Process(Process&& other)
    : process_(other.process_.release()),
      is_current_process_(other.is_current_process_) {
  other.Close();
}

Process::~Process() {
}

Process& Process::operator=(Process&& other) {
  DCHECK_NE(this, &other);
  process_.Set(other.process_.release());
  is_current_process_ = other.is_current_process_;
  other.Close();
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
  return Process(::OpenProcess(kBasicProcessAccess, FALSE, pid));
}

// static
Process Process::OpenWithExtraPrivileges(ProcessId pid) {
  DWORD access = kBasicProcessAccess | PROCESS_DUP_HANDLE | PROCESS_VM_READ;
  return Process(::OpenProcess(access, FALSE, pid));
}

// static
Process Process::OpenWithAccess(ProcessId pid, DWORD desired_access) {
  return Process(::OpenProcess(desired_access, FALSE, pid));
}

// static
bool Process::CanSetPriority() {
  return true;
}

// static
void Process::TerminateCurrentProcessImmediately(int exit_code) {
#if BUILDFLAG(CLANG_PROFILING)
  WriteClangProfilingProfile();
#endif
  ::TerminateProcess(GetCurrentProcess(), static_cast<UINT>(exit_code));
  // There is some ambiguity over whether the call above can return. Rather than
  // hitting confusing crashes later on we should crash right here.
  ImmediateCrash();
}

bool Process::IsValid() const {
  return process_.is_valid() || is_current();
}

ProcessHandle Process::Handle() const {
  return is_current_process_ ? GetCurrentProcess() : process_.get();
}

Process Process::Duplicate() const {
  if (is_current())
    return Current();

  ProcessHandle out_handle;
  if (!IsValid() || !::DuplicateHandle(GetCurrentProcess(),
                                       Handle(),
                                       GetCurrentProcess(),
                                       &out_handle,
                                       0,
                                       FALSE,
                                       DUPLICATE_SAME_ACCESS)) {
    return Process();
  }
  return Process(out_handle);
}

ProcessHandle Process::Release() {
  if (is_current())
    return ::GetCurrentProcess();
  return process_.release();
}

ProcessId Process::Pid() const {
  DCHECK(IsValid());
  return GetProcId(Handle());
}

Time Process::CreationTime() const {
  FILETIME creation_time = {};
  FILETIME ignore1 = {};
  FILETIME ignore2 = {};
  FILETIME ignore3 = {};
  if (!::GetProcessTimes(Handle(), &creation_time, &ignore1, &ignore2,
                         &ignore3)) {
    return Time();
  }
  return Time::FromFileTime(creation_time);
}

bool Process::is_current() const {
  return is_current_process_;
}

void Process::Close() {
  is_current_process_ = false;
  if (!process_.is_valid())
    return;

  process_.Close();
}

bool Process::Terminate(int exit_code, bool wait) const {
  constexpr DWORD kWaitMs = 60 * 1000;

  DCHECK(IsValid());
  bool result =
      ::TerminateProcess(Handle(), static_cast<UINT>(exit_code)) != FALSE;
  if (result) {
    // The process may not end immediately due to pending I/O
    if (wait && ::WaitForSingleObject(Handle(), kWaitMs) != WAIT_OBJECT_0)
      DPLOG(ERROR) << "Error waiting for process exit";
    Exited(exit_code);
  } else {
    // The process can't be terminated, perhaps because it has already exited or
    // is in the process of exiting. An error code of ERROR_ACCESS_DENIED is the
    // undocumented-but-expected result if the process has already exited or
    // started exiting when TerminateProcess is called, so don't print an error
    // message in that case.
    if (GetLastError() != ERROR_ACCESS_DENIED)
      DPLOG(ERROR) << "Unable to terminate process";
    // A non-zero timeout is necessary here for the same reasons as above.
    if (::WaitForSingleObject(Handle(), kWaitMs) == WAIT_OBJECT_0) {
      DWORD actual_exit;
      Exited(::GetExitCodeProcess(Handle(), &actual_exit)
                 ? static_cast<int>(actual_exit)
                 : exit_code);
      result = true;
    }
  }
  return result;
}

Process::WaitExitStatus Process::WaitForExitOrEvent(
    const base::win::ScopedHandle& stop_event_handle,
    int* exit_code) const {
  HANDLE events[] = {Handle(), stop_event_handle.get()};
  DWORD wait_result =
      ::WaitForMultipleObjects(std::size(events), events, FALSE, INFINITE);

  if (wait_result == WAIT_OBJECT_0) {
    DWORD temp_code;  // Don't clobber out-parameters in case of failure.
    if (!::GetExitCodeProcess(Handle(), &temp_code))
      return Process::WaitExitStatus::FAILED;

    if (exit_code)
      *exit_code = static_cast<int>(temp_code);

    Exited(static_cast<int>(temp_code));
    return Process::WaitExitStatus::PROCESS_EXITED;
  }

  if (wait_result == WAIT_OBJECT_0 + 1) {
    return Process::WaitExitStatus::STOP_EVENT_SIGNALED;
  }

  return Process::WaitExitStatus::FAILED;
}

bool Process::WaitForExit(int* exit_code) const {
  return WaitForExitWithTimeout(TimeDelta::Max(), exit_code);
}

bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const {
  TRACE_EVENT0("base", "Process::WaitForExitWithTimeout");

  if (!timeout.is_zero()) {
    // Assert that this thread is allowed to wait below. This intentionally
    // doesn't use ScopedBlockingCallWithBaseSyncPrimitives because the process
    // being waited upon tends to itself be using the CPU and considering this
    // thread non-busy causes more issue than it fixes: http://crbug.com/905788
    internal::AssertBaseSyncPrimitivesAllowed();
  }

  // Limit timeout to INFINITE.
  DWORD timeout_ms = saturated_cast<DWORD>(timeout.InMilliseconds());
  if (::WaitForSingleObject(Handle(), timeout_ms) != WAIT_OBJECT_0)
    return false;

  DWORD temp_code;  // Don't clobber out-parameters in case of failure.
  if (!::GetExitCodeProcess(Handle(), &temp_code))
    return false;

  if (exit_code)
    *exit_code = static_cast<int>(temp_code);

  Exited(static_cast<int>(temp_code));
  return true;
}

void Process::Exited(int exit_code) const {}

Process::Priority Process::GetPriority() const {
  DCHECK(IsValid());
  int priority = GetOSPriority();
  if (priority == 0)
    return Priority::kUserBlocking;  // Failure case. Use default value.
  if ((priority == BELOW_NORMAL_PRIORITY_CLASS) ||
      (priority == IDLE_PRIORITY_CLASS)) {
    return Priority::kBestEffort;
  }

  PROCESS_POWER_THROTTLING_STATE power_throttling = {
      .Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION,
      .ControlMask = 0ul,
      .StateMask = 0ul,
  };
  const bool ret =
      ::GetProcessInformation(Handle(), ProcessPowerThrottling,
                              &power_throttling, sizeof(power_throttling));

  // Return Priority::kUserVisible if EcoQoS read & write supported and level
  // set.
  if (ret != 0 &&
      power_throttling.ControlMask ==
          PROCESS_POWER_THROTTLING_EXECUTION_SPEED &&
      power_throttling.StateMask == PROCESS_POWER_THROTTLING_EXECUTION_SPEED) {
    return Priority::kUserVisible;
  }

  return Priority::kUserBlocking;
}

bool Process::SetPriority(Priority priority) {
  DCHECK(IsValid());
  // Having a process remove itself from background mode is a potential
  // priority inversion, and having a process put itself in background mode is
  // broken in Windows 11 22H2. So, it is no longer supported. See
  // https://crbug.com/1396155 for details.
  DCHECK(!is_current());
  const DWORD priority_class = priority == Priority::kBestEffort
                                   ? IDLE_PRIORITY_CLASS
                                   : NORMAL_PRIORITY_CLASS;

  auto* os_info = base::win::OSInfo::GetInstance();
  if (os_info->version() >= win::Version::WIN11) {
    PROCESS_POWER_THROTTLING_STATE power_throttling;
    RtlZeroMemory(&power_throttling, sizeof(power_throttling));
    power_throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;

    // EcoQoS is a Windows 11 only feature, but before 22H2, there is no way to
    // query its current QoS state, GetProcessInformation API to read
    // PROCESS_POWER_THROTTLING_STATE would fail. For kUserVisible, we
    // intentionally exclude clients before 22H2 so that GetPriority() is
    // consistent with SetPriority().
    if ((priority == Priority::kBestEffort &&
         FeatureList::IsEnabled(kUseEcoQoSForBackgroundProcess)) ||
        (priority == Priority::kUserVisible &&
         os_info->version() >= win::Version::WIN11_22H2)) {
      // Sets Eco QoS level.
      power_throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
      power_throttling.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    } else {
      // Uses system default.
      power_throttling.ControlMask = 0;
      power_throttling.StateMask = 0;
    }
    bool ret =
        ::SetProcessInformation(Handle(), ProcessPowerThrottling,
                                &power_throttling, sizeof(power_throttling));
    if (ret == 0) {
      DPLOG(ERROR) << "Setting process QoS policy fails";
    }
  }

  return (::SetPriorityClass(Handle(), priority_class) != 0);
}

int Process::GetOSPriority() const {
  DCHECK(IsValid());
  return static_cast<int>(::GetPriorityClass(Handle()));
}

}  // namespace base

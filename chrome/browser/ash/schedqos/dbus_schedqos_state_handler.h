// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCHEDQOS_DBUS_SCHEDQOS_STATE_HANDLER_H_
#define CHROME_BROWSER_ASH_SCHEDQOS_DBUS_SCHEDQOS_STATE_HANDLER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/process/process_priority_delegate.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/cross_process_platform_thread_delegate.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_type_delegate.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/system/procfs_util.h"
#include "dbus/dbus_result.h"

namespace ash {
// DBusSchedQOSStateHandler sends `SetProcessState` and `SetThreadState` DBus
// requests to resourced on `base::Process::SetPriority()` and
// `base::PlatformThread::SetThreadType()` instead of updating OS scheduler
// settings directly.
//
// DBusSchedQOSStateHandler is for ChromeOS only.
//
// DBusSchedQOSStateHandler caches process priorities to be consistent with
// `base::Process::GetPriority()`. Processes which will change their priorities
// need to call `base::Process::InitializePriority()` before calling
// `base::Process::SetPriority()`. Otherwise `base::Process::SetPriority()`
// fails. Also the processes must call `base::Process::ForgetPriority()` when
// they terminate. Otherwise the cache in this class leaks.
class DBusSchedQOSStateHandler
    : public base::ProcessPriorityDelegate,
      public base::ThreadTypeDelegate,
      public base::CrossProcessPlatformThreadDelegate {
 public:
  DBusSchedQOSStateHandler(const DBusSchedQOSStateHandler&) = delete;
  DBusSchedQOSStateHandler& operator=(const DBusSchedQOSStateHandler&) = delete;

  ~DBusSchedQOSStateHandler() override;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PidReuseResult {
    kNotPidReuseOnFail = 0,
    kPidReuseOnFail = 1,
    kNotPidReuseOnSuccess = 2,
    kPidReuseOnSuccess = 3,
    kMaxValue = kPidReuseOnSuccess,
  };

  // Creates a SandboxedProcessThreadTypeHandler instance and stores it to
  // g_instance. Make sure the g_instance doesn't exist before creation.
  //
  // `main_task_runner` is the main thread's task runner to call D-Bus clients
  // from.
  static DBusSchedQOSStateHandler* Create(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner);

  // base::ProcessPriorityDelegate :
  bool CanSetProcessPriority() override;
  void InitializeProcessPriority(base::ProcessId process_id) override;
  void ForgetProcessPriority(base::ProcessId process_id) override;
  bool SetProcessPriority(base::ProcessId process_id,
                          base::Process::Priority priority) override;

  base::Process::Priority GetProcessPriority(
      base::ProcessId process_id) override;

  bool HandleThreadTypeChange(base::ProcessId process_id,
                              base::PlatformThreadId thread_id,
                              base::ThreadType thread_type) override;
  bool HandleThreadTypeChange(base::PlatformThreadId thread_id,
                              base::ThreadType thread_type) override;

 private:
  struct ProcessState {
    base::Process::Priority priority;
    bool need_retry;
    std::map<base::PlatformThreadId, base::ThreadType>
        preconnected_thread_types;

    explicit ProcessState(base::Process::Priority priority);
    ProcessState() = delete;
    ProcessState(base::Process::Priority priority, bool need_retry);
    ~ProcessState();
    ProcessState(ProcessState&&);
    ProcessState(ProcessState&) = delete;
  };

  explicit DBusSchedQOSStateHandler(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner);

  void WaitForResourcedAvailable();

  void CheckResourcedDisconnected(dbus::DBusResult result);

  void OnServiceConnected(bool success);

  void SetProcessPriorityOnThread(base::ProcessId process_id,
                                  base::Process::Priority priority);

  void OnSetProcessPriorityFinish(base::ProcessId process_id,
                                  base::Process::Priority priority,
                                  base::ElapsedTimer elapsed_timer,
                                  system::ProcStatFile stat_file,
                                  dbus::DBusResult result);

  void MarkProcessToRetry(base::ProcessId process_id);

  void SetThreadTypeOnThread(base::ProcessId process_id,
                             base::PlatformThreadId thread_id,
                             base::ThreadType thread_type);

  void OnSetThreadTypeFinish(base::ProcessId process_id,
                             base::PlatformThreadId thread_id,
                             base::ThreadType thread_type,
                             base::ElapsedTimer elapsed_timer,
                             system::ProcStatFile stat_file,
                             dbus::DBusResult result);

  void AddThreadRetryEntry(base::ProcessId process_id,
                           base::PlatformThreadId thread_id,
                           base::ThreadType thread_type);

  SEQUENCE_CHECKER(sequence_checker_);

  bool is_connected_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_dbus_down_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::Lock process_state_map_lock_;

  std::map<base::ProcessId, ProcessState> process_state_map_
      GUARDED_BY(process_state_map_lock_);

  // ResourcedClient need to be called on the main thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  base::WeakPtrFactory<DBusSchedQOSStateHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCHEDQOS_DBUS_SCHEDQOS_STATE_HANDLER_H_

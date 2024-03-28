// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_SCHEDQOS_STATE_HANDLER_H_
#define CHROME_BROWSER_ASH_DBUS_SCHEDQOS_STATE_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/process/process_priority_delegate.h"
#include "base/task/sequenced_task_runner.h"
#include "dbus/dbus_result.h"

namespace ash {
// DBusSchedQOSStateHandler sends `SetProcessState` and `SetThreadState` DBus
// requests to resourced on `base::Process::SetPriority()` and
// `base::PlatformThread::SetThreadType()` instead of updating OS scheduler
// settings directly.
//
// DBusSchedQOSStateHandler is for ChromeOS only.
class DBusSchedQOSStateHandler : public base::ProcessPriorityDelegate {
 public:
  DBusSchedQOSStateHandler(const DBusSchedQOSStateHandler&) = delete;
  DBusSchedQOSStateHandler& operator=(const DBusSchedQOSStateHandler&) = delete;

  ~DBusSchedQOSStateHandler() override;

  // Creates a SandboxedProcessThreadTypeHandler instance and stores it to
  // g_instance. Make sure the g_instance doesn't exist before creation.
  //
  // `main_task_runner` is the main thread's task runner to call D-Bus clients
  // from.
  static DBusSchedQOSStateHandler* Create(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner);

  // base::ProcessPriorityDelegate :
  bool CanSetProcessPriority() override;
  bool SetProcessPriority(base::ProcessId process_id,
                          base::Process::Priority priority) override;

 private:
  explicit DBusSchedQOSStateHandler(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner);

  void SetProcessPriorityOnThread(base::ProcessId process_id,
                                  base::Process::Priority priority);

  void OnSetProcessPriorityFinish(base::ProcessId process_id,
                                  base::Process::Priority priority,
                                  dbus::DBusResult result);

  // ResourcedClient need to be called on the main thread.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  base::WeakPtrFactory<DBusSchedQOSStateHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_SCHEDQOS_STATE_HANDLER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/windows_types.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_switches.h"

namespace platform_experience {

namespace {

// Callbacks must never be dependant on the `DelegatedTaskRunner` instance as
// they can run after the runner instance is destroyed.
void ReturnTaskCompletionStatusAsync(
    DelegatedTaskExitCodeOrStatus exit_code_or_status,
    base::TimeDelta execution_time,
    DelegatedTaskCompletionCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          DelegatedTaskResult{std::move(exit_code_or_status), execution_time}));
}

}  // namespace

DelegatedTaskRunner::DelegatedTaskRunner()
    : DelegatedTaskRunner(std::make_unique<PehLauncher>()) {}

DelegatedTaskRunner::DelegatedTaskRunner(
    std::unique_ptr<PehLauncher> peh_launcher)
    : peh_launcher_(base::ThreadPool::CreateSequencedTaskRunner(
                        {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
                    std::move(peh_launcher)) {
  CHECK(!peh_launcher_.is_null());
}

DelegatedTaskRunner::~DelegatedTaskRunner() {
  if (task_) {
    CleanupAndReturnResult(base::unexpected(
        DelegatedTaskStatus::kRunnerDestroyedBeforeTaskCompletion));
  }
}

void DelegatedTaskRunner::Run(std::unique_ptr<DelegatedTask> task,
                              DelegatedTaskCompletionCallback callback) {
  CHECK(task_start_time_.is_null());

  task_ = std::move(task);
  task_start_time_ = base::TimeTicks::Now();
  completion_callback_ = std::move(callback);

  // Start the timeout timer from task initialization so the timeout duration
  // and recorded `execution_time` track the same time slice from task start.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DelegatedTaskRunner::CleanupAndReturnResult,
                     weak_factory_.GetWeakPtr(),
                     base::unexpected(DelegatedTaskStatus::kTaskTimeout)),
      task_->GetTimeout());

  // TODO(b/525018453): Verify the binary after fetching the path.
  peh_launcher_.AsyncCall(&PehLauncher::GetBinaryPath)
      .Then(base::BindOnce(&DelegatedTaskRunner::OnBinaryPathRetrieved,
                           weak_factory_.GetWeakPtr()));
}

void DelegatedTaskRunner::OnBinaryPathRetrieved(
    const base::FilePath& peh_binary_path) {
  if (peh_binary_path.empty()) {
    CleanupAndReturnResult(base::unexpected(DelegatedTaskStatus::kPehNotFound));
    return;
  }

  base::CommandLine cmd_line = base::CommandLine(peh_binary_path);
  cmd_line.AppendSwitchASCII(kDelegatedTasksSwitch, task_->GetTaskName());
  task_->AppendCommandLineSwitches(cmd_line);

  peh_launcher_.AsyncCall(&PehLauncher::LaunchProcess)
      .WithArgs(cmd_line, base::LaunchOptions())
      .Then(base::BindOnce(
          [](base::WeakPtr<DelegatedTaskRunner> runner, base::Process process) {
            if (runner) {
              runner->OnProcessLaunched(std::move(process));
            } else if (process.IsValid()) {
              process.Terminate(/*exit_code=*/1, /*wait=*/false);
            }
          },
          weak_factory_.GetWeakPtr()));
}

void DelegatedTaskRunner::OnProcessLaunched(base::Process process) {
  process_ = std::move(process);
  if (!process_.IsValid()) {
    CleanupAndReturnResult(
        base::unexpected(DelegatedTaskStatus::kProcessLaunchFailure));
    return;
  }

  if (!watcher_.StartWatchingOnce(process_.Handle(), this)) {
    CleanupAndReturnResult(
        base::unexpected(DelegatedTaskStatus::kWatchProcessHandleFailure));
    return;
  }
}

void DelegatedTaskRunner::OnObjectSignaled(HANDLE object) {
  DWORD exit_code = 0;
  DelegatedTaskExitCodeOrStatus exit_code_or_status;

  if (::GetExitCodeProcess(process_.Handle(), &exit_code)) {
    switch (exit_code) {
      case static_cast<int>(PehExitCode::kInvalidTaskType):
        exit_code_or_status =
            base::unexpected(DelegatedTaskStatus::kInvalidTaskType);
        break;
      case static_cast<int>(PehExitCode::kInvalidArgs):
        exit_code_or_status =
            base::unexpected(DelegatedTaskStatus::kInvalidArgs);
        break;
      default:
        exit_code_or_status = static_cast<int>(exit_code);
        break;
    }
  } else {
    exit_code_or_status =
        base::unexpected(DelegatedTaskStatus::kInvalidExitCode);
  }

  CleanupAndReturnResult(std::move(exit_code_or_status));
}

void DelegatedTaskRunner::CleanupAndReturnResult(
    DelegatedTaskExitCodeOrStatus exit_code_or_status) {
  watcher_.StopWatching();

  if (process_.IsValid() && process_.IsRunning()) {
    process_.Terminate(/*exit_code=*/1, /*wait=*/false);
  }

  weak_factory_.InvalidateWeakPtrs();
  task_.reset();

  CHECK(completion_callback_);
  base::TimeDelta execution_time = base::TimeTicks::Now() - task_start_time_;
  ReturnTaskCompletionStatusAsync(std::move(exit_code_or_status),
                                  execution_time,
                                  std::move(completion_callback_));

  // TODO(b/525017787): Add UMA telemetry to log task result.
}

}  // namespace platform_experience

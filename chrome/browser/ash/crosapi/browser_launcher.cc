// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace crosapi {

namespace {

void TerminateProcessBackground(base::Process process,
                                base::TimeDelta timeout) {
  // Here, lacros-chrome process may crash, or be in the shutdown procedure.
  // Give some amount of time for the collection. In most cases,
  // this waits until it captures the process termination.
  if (process.WaitForExitWithTimeout(timeout, nullptr)) {
    return;
  }

  // Here, the process is not yet terminated.
  // This happens if some critical error happens on the mojo connection,
  // while both ash-chrome and lacros-chrome are still alive.
  // Terminate the lacros-chrome.
  bool success = process.Terminate(/*exit_code=*/0, /*wait=*/true);
  LOG_IF(ERROR, !success) << "Failed to terminate the lacros-chrome.";
}

}  // namespace

BrowserLauncher::BrowserLauncher() = default;

BrowserLauncher::~BrowserLauncher() = default;

bool BrowserLauncher::LaunchProcess(const base::CommandLine& command_line,
                                    const base::LaunchOptions& options) {
  // If `process_` is already launched once before, it should be reset to
  // invalid inside HandleReload before calling LaunchProcess.
  CHECK(!process_.IsValid());
  process_ = base::LaunchProcess(command_line, options);
  if (!process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    return false;
  }
  LOG(WARNING) << "Launched lacros-chrome with pid " << process_.Pid();
  return true;
}

bool BrowserLauncher::IsProcessValid() {
  return process_.IsValid();
}

bool BrowserLauncher::TriggerTerminate(int exit_code) {
  if (!process_.IsValid()) {
    return false;
  }

  process_.Terminate(/*exit_code=*/exit_code, /*wait=*/false);

  // TODO(mayukoaiba): We should reset `process_` by base::Process() in order to
  // manage the state of process properly
  return true;
}

void BrowserLauncher::EnsureProcessTerminated(base::OnceClosure callback,
                                              base::TimeDelta timeout) {
  CHECK(process_.IsValid());
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&TerminateProcessBackground, std::move(process_), timeout),
      std::move(callback));
  CHECK(!process_.IsValid());
}

const base::Process& BrowserLauncher::GetProcessForTesting() {
  return process_;
}

}  // namespace crosapi

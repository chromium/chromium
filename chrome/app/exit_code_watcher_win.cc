// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/exit_code_watcher_win.h"

#include <windows.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"

namespace {

constexpr char kBrowserExitCodeHistogramName[] = "Stability.BrowserExitCodes";

bool WriteProcessExitCode(int exit_code) {
  if (exit_code != STILL_ACTIVE) {
    // Record the exit codes in a sparse stability histogram, as the range of
    // values used to report failures is large.
    base::HistogramBase* exit_code_histogram =
        base::SparseHistogram::FactoryGet(
            kBrowserExitCodeHistogramName,
            base::HistogramBase::kUmaStabilityHistogramFlag);
    exit_code_histogram->Add(exit_code);
    return true;
  }
  return false;
}

}  // namespace

ExitCodeWatcher::ExitCodeWatcher()
    : background_thread_("ExitCodeWatcherThread"),
      exit_code_(STILL_ACTIVE),
      stop_watching_handle_(::CreateEvent(nullptr, TRUE, FALSE, nullptr)) {
  DCHECK(stop_watching_handle_.IsValid());
}

ExitCodeWatcher::~ExitCodeWatcher() = default;

bool ExitCodeWatcher::Initialize(base::Process process) {
  if (!process.IsValid()) {
    return false;
  }

  DWORD process_pid = process.Pid();
  if (process_pid == 0) {
    return false;
  }

  FILETIME creation_time = {};
  FILETIME dummy = {};
  if (!::GetProcessTimes(process.Handle(), &creation_time, &dummy, &dummy,
                         &dummy)) {
    return false;
  }

  // Success, take ownership of the process.
  process_ = std::move(process);
  return true;
}

bool ExitCodeWatcher::StartWatching() {
  if (!background_thread_.StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0))) {
    return false;
  }

  // Unretained is safe because `this` owns the thread.
  if (!background_thread_.task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&ExitCodeWatcher::WaitForExit,
                                    base::Unretained(this)))) {
    background_thread_.Stop();
    return false;
  }

  return true;
}

void ExitCodeWatcher::StopWatching() {
  if (stop_watching_handle_.IsValid()) {
    ::SetEvent(stop_watching_handle_.Get());
  }
}

void ExitCodeWatcher::WaitForExit() {
  base::Process::WaitExitStatus wait_result =
      process_.WaitForExitOrEvent(stop_watching_handle_, &exit_code_);
  if (wait_result == base::Process::WaitExitStatus::PROCESS_EXITED) {
    WriteProcessExitCode(exit_code_);
  }
}

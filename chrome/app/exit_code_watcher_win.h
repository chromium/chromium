// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_APP_EXIT_CODE_WATCHER_WIN_H_
#define CHROME_APP_EXIT_CODE_WATCHER_WIN_H_

#include "base/process/process.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"

// Watches for the exit code of a process and records
class ExitCodeWatcher {
 public:
  ExitCodeWatcher();

  ExitCodeWatcher(const ExitCodeWatcher&) = delete;
  ExitCodeWatcher& operator=(const ExitCodeWatcher&) = delete;

  ~ExitCodeWatcher();

  // This function expects |process| to be open with sufficient privilege to
  // wait and retrieve the process exit code.
  // It checks the handle for validity and takes ownership of it.
  bool Initialize(base::Process process);

  bool StartWatching();

  void StopWatching();

  const base::Process& process() const { return process_; }
  int ExitCodeForTesting() const { return exit_code_; }

 private:
  // Waits for the process to exit and records its exit code in a histogram.
  // This is a blocking call.
  void WaitForExit();

  // Watched process and its creation time.
  base::Process process_;

  // The thread that runs WaitForExit().
  base::Thread background_thread_;

  // The exit code of the watched process. Valid after WaitForExit.
  int exit_code_;

  // Event handle to use to stop exit watcher thread
  base::win::ScopedHandle stop_watching_handle_;
};

#endif  // CHROME_APP_EXIT_CODE_WATCHER_WIN_H_

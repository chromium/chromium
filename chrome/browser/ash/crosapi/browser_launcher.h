// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_

#include "base/functional/callback_forward.h"
#include "base/process/process.h"
#include "base/time/time.h"

namespace base {
class CommandLine;
struct LaunchOptions;
}  // namespace base

namespace crosapi {

// Manages launching and terminating Lacros process.
// TODO(crbug.com/1495590): Extract launching logic from BrowserManager to
// BrowserLauncher.
class BrowserLauncher {
 public:
  BrowserLauncher();

  BrowserLauncher(const BrowserLauncher&) = delete;
  BrowserLauncher& operator=(const BrowserLauncher&) = delete;

  ~BrowserLauncher();

  // Launches a process of the given command_line and options, which are
  // expected to be Lacros's ones.
  bool LaunchProcess(const base::CommandLine& command_line,
                     const base::LaunchOptions& options);

  // Returns true if process is valid.
  bool IsProcessValid();

  // Triggers termination synchronously if process is running.
  // Does not block the thread because it does not wait for the process
  // termination.
  bool TriggerTerminate(int exit_code);

  // Waits for termination of the running process asynchronously during the
  // period given by the `timeout`, then invoke `callback`. On timeout, also
  // tries to terminate the process by sending a signal.
  // TODO(mayukoaiba): On calling this function, even before the termination
  // procedure is completed (i.e. before `callback` is called), IsProcessValid
  // will return false and LaunchProcess tries to create the next process, which
  // may be confusing for callers. We should fix this issue.
  void EnsureProcessTerminated(base::OnceClosure callback,
                               base::TimeDelta timeout);

  // Returns reference to `process_` for testing.
  const base::Process& GetProcessForTesting();

 private:
  // Process handle for the lacros_chrome process.
  base::Process process_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_

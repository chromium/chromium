// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_LAUNCHER_TEST_LAUNCHER_FORK_SERVER_H_
#define BASE_TEST_LAUNCHER_TEST_LAUNCHER_FORK_SERVER_H_

#include <stddef.h>

#include <optional>

#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base {

class CommandLine;
class TestLauncherDelegate;

// Starts an early fork server process if enabled by default (or
// unconditionally when `force_for_testing` is true).
// Returns false in the parent launcher process (or if the fork server was not
// started), and returns true in each newly forked child test process after
// updating `*argc`, `*argv`, and `CommandLine::ForCurrentProcess()`.
bool MaybeStartTestLauncherForkServer(int* argc,
                                      char*** argv,
                                      bool force_for_testing);

// Stops the fork server process (if running) and reaps it.
void StopTestLauncherForkServer();

// Marks the fork server as shutting down so any concurrently spawning test
// process is immediately terminated rather than outliving
// `KillSpawnedTestProcesses()`.
void NotifyTestLauncherForkServerShutdown();

// Returns true if the fork server is running and can be used to launch the
// given single-test command line.
bool CanUseTestLauncherForkServer(const CommandLine& command_line,
                                  size_t num_tests,
                                  TestLauncherDelegate* delegate);

// Launches a single-test child process via the fork server and waits for it to
// finish or time out. Returns the exit code of the process (or -1 on post-spawn
// error or timeout, setting `*was_timeout = true` on timeout), or
// `std::nullopt` if pre-spawn IPC to the fork server failed so the caller can
// fall back to `LaunchProcess()`.
std::optional<int> LaunchChildTestProcessViaForkServer(
    const CommandLine& command_line,
    const LaunchOptions& options,
    TimeDelta timeout_per_test,
    TestLauncherDelegate* delegate,
    bool* was_timeout,
    Lock* live_processes_lock,
    absl::flat_hash_map<ProcessHandle, CommandLine>* live_processes);

}  // namespace base

#endif  // BASE_TEST_LAUNCHER_TEST_LAUNCHER_FORK_SERVER_H_

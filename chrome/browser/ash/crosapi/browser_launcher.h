// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/process/process.h"
#include "base/time/time.h"

namespace base {
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

  // Returns specific path for saving Lacros logs, depending on which images are
  // used.
  static base::FilePath LacrosLogDirectory();

  // Parameters used to launch Lacros that are calculated on a background
  // sequence.
  struct LaunchParamsFromBackground {
   public:
    LaunchParamsFromBackground();
    LaunchParamsFromBackground(LaunchParamsFromBackground&&);
    LaunchParamsFromBackground& operator=(LaunchParamsFromBackground&&);
    LaunchParamsFromBackground(const LaunchParamsFromBackground&) = delete;
    LaunchParamsFromBackground& operator=(const LaunchParamsFromBackground&) =
        delete;
    ~LaunchParamsFromBackground();

    // An fd for a log file.
    base::ScopedFD logfd;

    // Sets true if Lacros uses resource file sharing.
    bool enable_resource_file_sharing = false;

    // Sets true if Lacros uses a shared components directory.
    bool enable_shared_components_dir = false;

    // Any additional args to start lacros with.
    std::vector<std::string> lacros_additional_args;
  };

  // Launches a process of the given options, which are expected to be Lacros's
  // ones.
  bool LaunchProcess(const base::FilePath& chrome_path,
                     const LaunchParamsFromBackground& params,
                     bool launching_at_login_screen,
                     std::optional<int> startup_data_fd,
                     std::optional<int> postlogin_data_fd,
                     std::string_view channel_flag_value,
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

  // Returns command line from command line initialized by
  // `InitializeParameters` for unit test.
  base::CommandLine InitializeParametersForTesting(
      const base::FilePath& chrome_path,
      const LaunchParamsFromBackground& params,
      bool launching_at_login_screen,
      std::optional<int> startup_data_fd,
      std::optional<int> postlogin_data_fd,
      std::string_view channel_flag_value);

  // Returns reference to `process_` for testing.
  const base::Process& GetProcessForTesting();

  // Makes `LaunchProcessWithParameters` usable within the unit tests.
  bool LaunchProcessForTesting(const base::CommandLine& command_line,
                               const base::LaunchOptions& options);

 private:
  // Initializes argv for making the command line.
  // TODO(mayukoaiba): The process of making `command_line` is separated into 2
  // parts (initializing with argv and appending to commandline) because of
  // historical reasons. We should combine and unify them.
  std::vector<std::string> InitializeArgv(
      const base::FilePath& chrome_path,
      const LaunchParamsFromBackground& params,
      bool launching_at_login_screen,
      std::optional<int> startup_data_fd,
      std::optional<int> postlogin_data_fd);

  // Initializes the command line for launching Lacros.
  base::CommandLine InitializeParameters(
      const base::FilePath& chrome_path,
      const LaunchParamsFromBackground& params,
      bool launching_at_login_screen,
      std::optional<int> startup_data_fd,
      std::optional<int> postlogin_data_fd,
      std::string_view channel_flag_value);

  // Launches a process , which is excuted in `LaunchProcess`.
  // This is also used for unittest.
  bool LaunchProcessWithParameters(const base::CommandLine& command_line,
                                   const base::LaunchOptions& options);

  // Process handle for the lacros_chrome process.
  base::Process process_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_

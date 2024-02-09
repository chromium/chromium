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
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/common/channel_info.h"
#include "components/nacl/common/buildflags.h"
#include "components/policy/core/common/values_util.h"
#include "mojo/public/cpp/platform/platform_channel.h"

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

    // Sets true if Lacros forks Zygotes at login screen.
    bool enable_fork_zygotes_at_login_screen = false;

    // Any additional args to start lacros with.
    std::vector<std::string> lacros_additional_args;
  };

  // Parameters to handle command line and options used to launching Lacros.
  struct LaunchParams {
   public:
    LaunchParams(base::CommandLine command_line, base::LaunchOptions options);
    LaunchParams(LaunchParams&&);
    LaunchParams& operator=(LaunchParams&&);
    LaunchParams(const LaunchParams&) = delete;
    LaunchParams& operator=(const LaunchParams&) = delete;
    ~LaunchParams();

    base::CommandLine command_line;

    base::LaunchOptions options;
  };

  // Results from `LaunchProcess` and needs to be passed to
  // BrowserManager.
  struct LaunchResults {
   public:
    LaunchResults();
    LaunchResults(LaunchResults&&);
    LaunchResults& operator=(LaunchResults&&);
    LaunchResults(const LaunchResults&) = delete;
    LaunchResults& operator=(const LaunchResults&) = delete;
    ~LaunchResults();

    // ID for the current Crosapi connection.
    // Available only when lacros-chrome is running.
    CrosapiId crosapi_id;

    // Time when the lacros process was launched.
    base::TimeTicks lacros_launch_time;
  };

  // Launches a process of the given options, which are expected to be Lacros's
  // ones.
  // Following is explanation for Arguments.
  // `chrome_path`: Initializes `command_line`.
  // `params`: Parameters used to launch Lacros that are calculated on a
  // background sequence.
  // `launching_at_login_screen`: Whether lacros is launching at login screen.
  // `postlogin_pipe_fd`: Pipe FDs through which Ash and Lacros exchange
  // post-login parameters.
  // `lacros_selection`: Whether "rootfs" or "stateful" lacros is selected.
  // `mojo_disconnection_cb`: Callback function setting up mojo connection.
  // `BrowserManager::OnMojoDisconnected` is called.
  // `is_keep_alive_enabled`: Whether `keep_alive_features` is empty.
  std::optional<LaunchResults> LaunchProcess(
      const base::FilePath& chrome_path,
      const LaunchParamsFromBackground& params,
      bool launching_at_login_screen,
      browser_util::LacrosSelection lacros_selection,
      base::OnceClosure mojo_disconnection_cb,
      bool is_keep_alive_enabled);

  // Writes post login data to the Lacros process. After that,
  // `postlogin_pipe_fd` is reset.
  void ResumeLaunch();

  void SetLastPolicyFetchAttemptTimestamp(base::Time last_refresh);

  EnvironmentProvider& environment_provider() { return environment_provider_; }

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

  // Makes `LaunchProcessWithParameters` usable within the unit tests.
  bool LaunchProcessForTesting(const LaunchParams& parameters);

  // Sets up additional flags for unit tests.
  // This function overwrites `command_line` with the desired flags.
  void SetUpAdditionalParametersForTesting(LaunchParamsFromBackground& params,
                                           LaunchParams& parameters);

 private:
  LaunchParams CreateLaunchParams(
      const base::FilePath& chrome_path,
      const LaunchParamsFromBackground& params,
      bool launching_at_login_screen,
      std::optional<int> startup_fd,
      std::optional<int> read_pipe_fd,
      mojo::PlatformChannel& channel,
      browser_util::LacrosSelection lacros_selection);

  // Launches a process , which is executed in `LaunchProcess`.
  // This is also used for unittest.
  bool LaunchProcessWithParameters(const LaunchParams& parameters);

  // Process handle for the lacros_chrome process.
  base::Process process_;

  // Pipe FDs through which Ash and Lacros exchange post-login parameters.
  base::ScopedFD postlogin_pipe_fd_;

  // Used to pass ash-chrome specific flags/configurations to lacros-chrome.
  EnvironmentProvider environment_provider_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_

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
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "components/nacl/common/buildflags.h"
#include "components/policy/core/common/values_util.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace base {
struct LaunchOptions;
}  // namespace base

namespace user_manager {
class DeviceOwnershipWaiter;
}  // namespace user_manager

namespace crosapi {
class PrimaryProfileCreationWaiter;

// Manages launching and terminating Lacros process.
// TODO(crbug.com/40286595): Extract launching logic from BrowserManager to
// BrowserLauncher.
class BrowserLauncher {
 public:
  BrowserLauncher();

  BrowserLauncher(const BrowserLauncher&) = delete;
  BrowserLauncher& operator=(const BrowserLauncher&) = delete;

  ~BrowserLauncher();

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

  // Reason of Lacros not being able to launch.
  enum class LaunchFailureReason {
    // Failed to launch due to unknown error.
    kUnknown,

    // Shutdown is requested from BrowserManager during the process launch.
    kShutdownRequested,
  };

  using LaunchCompletionCallback = base::OnceCallback<void(
      base::expected<LaunchResults, LaunchFailureReason>)>;

  // Launches a process of the given options, which are expected to be Lacros's
  // ones.
  // `Launch` will:
  // 1. Prepare launching on background thread to handle blocking resources.
  // 2. Wait for device owner and primary user if it's launching in the user
  // session.
  // 3. Launch lacros process with the params prepared at Step 1.
  //
  // Following is explanation for Arguments.
  // `chrome_path`: Initializes `command_line`.
  // `lacros_selection`: Whether "rootfs" or "stateful" lacros is selected.
  // `mojo_disconnection_cb`: Callback function setting up mojo connection.
  // `BrowserManager::OnMojoDisconnected` is called.
  // `is_keep_alive_enabled`: Whether `keep_alive_features` is empty.
  // `callback`: Callback function that will be called on launch process
  // completion.
  void Launch(const base::FilePath& chrome_path,
              ash::standalone_browser::LacrosSelection lacros_selection,
              base::OnceClosure mojo_disconnection_cb,
              bool is_keep_alive_enabled,
              LaunchCompletionCallback callback);

  // Writes post login data to the Lacros process, resets `postlogin_pipe_fd`
  // and then executes a callback.
  void ResumeLaunch(
      base::OnceCallback<
          void(base::expected<base::TimeTicks, LaunchFailureReason>)> callback);

  // Returns true if process is valid.
  bool IsProcessValid() const;

  // Triggers termination synchronously if process is running.
  // Does not block the thread because it does not wait for the process
  // termination.
  bool TriggerTerminate(int exit_code) const;

  // Waits for termination of the running process asynchronously during the
  // period given by the `timeout`, then invoke `callback`. On timeout, also
  // tries to terminate the process by sending a signal.
  // TODO(mayukoaiba): On calling this function, even before the termination
  // procedure is completed (i.e. before `callback` is called), IsProcessValid
  // will return false and LaunchProcess tries to create the next process, which
  // may be confusing for callers. We should fix this issue.
  void EnsureProcessTerminated(base::OnceClosure callback,
                               base::TimeDelta timeout);

  // Records Shutdown() request from BrowserManager.
  void Shutdown() { shutdown_requested_ = true; }

  // Returns reference to `process_` for testing.
  const base::Process& GetProcessForTesting() const;

  // Provides public API to call LaunchProcessWithParameters for testing.
  bool LaunchProcessForTesting(const LaunchParams& parameters);

  // Provides public API to call CreateLaunchParamsForTesting for testing.
  LaunchParams CreateLaunchParamsForTesting(
      const base::FilePath& chrome_path,
      const LaunchParamsFromBackground& params,
      std::optional<int> startup_fd,
      mojo::PlatformChannel& channel,
      ash::standalone_browser::LacrosSelection lacros_selection);

  // Sets up additional flags for unit tests.
  // This function overwrites `command_line` with the desired flags.
  void SetUpAdditionalParametersForTesting(LaunchParamsFromBackground& params,
                                           LaunchParams& parameters) const;

  // Provides public API to call WaitForBackgroundWorkPreLaunch for testing.
  void WaitForBackgroundWorkPreLaunchForTesting(
      const base::FilePath& lacros_dir,
      bool clear_shared_resource_file,
      base::OnceClosure callback,
      LaunchParamsFromBackground& params);

  // TODO(crbug.com/40275396): Remove this once we refactored to use the
  // constructor.
  void set_device_ownership_waiter_for_testing(
      std::unique_ptr<user_manager::DeviceOwnershipWaiter>
          device_ownership_waiter);

  // Skips device ownership fetch. Use set_device_ownership_waiter_for_testing()
  // above if possible. Use this method only if your test must set up the
  // behavior before BrowserManager is initialized.
  // TODO(crbug.com/40275396): Remove this and set it from constructor.
  static void SkipDeviceOwnershipWaitForTesting(bool skip);

 private:
  // Waits for the prelaunch work running on background thread. `callback` is
  // called on background work completion and the output result is stored in
  // `params`.
  void WaitForBackgroundWorkPreLaunch(const base::FilePath& lacros_dir,
                                      bool clear_shared_resource_file,
                                      base::OnceClosure callback,
                                      LaunchParamsFromBackground& params);

  // Waits for the device owner being fetched from `UserManager` or the primary
  // user profile being fully created and then executes a callback.
  void WaitForDeviceOwnerFetchedAndThen(base::OnceClosure callback);
  void WaitForPrimaryProfileAddedAndThen(base::OnceClosure callback);

  // Launches lacros-chrome process after device owner and primary profile
  // become ready.
  void LaunchProcess(const base::FilePath& chrome_path,
                     std::unique_ptr<LaunchParamsFromBackground> params,
                     ash::standalone_browser::LacrosSelection lacros_selection,
                     base::OnceClosure mojo_disconnection_cb,
                     bool is_keep_alive_enabled,
                     LaunchCompletionCallback callback);

  LaunchParams CreateLaunchParams(
      const base::FilePath& chrome_path,
      const LaunchParamsFromBackground& params,
      std::optional<int> startup_fd,
      mojo::PlatformChannel& channel,
      ash::standalone_browser::LacrosSelection lacros_selection);

  // Launches a process , which is executed in `LaunchProcess`.
  // This is also used for unittest.
  bool LaunchProcessWithParameters(const LaunchParams& parameters);

  // Process handle for the lacros_chrome process.
  base::Process process_;

  // Used to delay an action until the definitive device owner is fetched.
  std::unique_ptr<user_manager::DeviceOwnershipWaiter> device_ownership_waiter_;

  // Used to wait for the primary user profile to be fully created.
  std::unique_ptr<PrimaryProfileCreationWaiter>
      primary_profile_creation_waiter_;

  // Tracks whether Shutdown() has been signalled by ash. This flag ensures any
  // new or existing lacros startup tasks are not executed during shutdown.
  bool shutdown_requested_ = false;

  // True if this is the first time that lacros is being launched from this ash
  // process. This value is used for resource sharing feature where ash deletes
  // cached shared resource file after ash is rebooted. Note that this flag
  // should not be reset on reloading as long as the ash process is not
  // relaunched.
  bool is_first_lacros_launch_ = true;

  // Indicates whether the delegate has been used.
  bool device_ownership_waiter_called_ = false;

  base::WeakPtrFactory<BrowserLauncher> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_LAUNCHER_H_

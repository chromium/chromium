// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_RUNNER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_RUNNER_WIN_H_

#include <limits>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
struct LaunchOptions;
}  // namespace base

namespace extensions {
class ExtensionRegistry;
class ExtensionService;
}  // namespace extensions

namespace safe_browsing {

class ChromeCleanerScannerResults;
class SwReporterInvocation;

// Class responsible for launching the cleaner process and waiting for its
// completion. This object is also responsible for managing the
// ChromePromptChannel object that handles IPC with the cleaner process.
//
// Expected lifecycle of a ChromeCleanerRunner:
//
//  - Instances are created via the static
//    RunChromeCleanerAndReplyWithExitCode() function. Instances will not be
//    destroyed before the Chrome Cleaner process has terminated _and_ a
//    connection-closed callback has been received. Destruction can happen on
//    any thread.
//  - The private LaunchAndWaitForExitOnBackgroundThread() function launches
//    the Chrome Cleaner process, creates a ChromePromptChannel object to
//    handle IPC communication and waits for the Cleaner process to terminate.
//    The static RunChromeCleanerAndReplyWithExitCode() function takes care of
//    calling that function correctly on a sequence with the correct traits.
//  - All callbacks registered with an instance are posted to run on the
//    provided |task_runner|.
//  - After a connection-closed has been received,
//    LaunchAndWaitForExitOnBackgroundThread() returns from the wait. On exit
//    from the function the last reference to ChromeCleanerRunner is dropped
//    and the ChromeCleanerRunner and ChromePromptChannel objects are
//    destroyed.
class ChromeCleanerRunner
    : public base::RefCountedThreadSafe<ChromeCleanerRunner> {
 public:
  enum class ChromeMetricsStatus {
    kEnabled,
    kDisabled,
  };

  enum class LaunchStatus {
    // Got an invalid process when attempting to launch the Chrome Cleaner
    // process. As a result, the IPC connection was never set up and the
    // |on_connection_closed| and |on_prompt_user| callbacks passed to
    // RunChromeCleanerAndReplyWithExitCode() will never be run.
    kLaunchFailed,
    // Waiting for the Chrome Cleaner process to exit failed.
    kLaunchSucceededFailedToWaitForCompletion,
    // Successfully waited for the Chrome Cleaner process to exit and received
    // the process's exit code.
    kSuccess,
  };

  struct ProcessStatus {
    LaunchStatus launch_status;
    // The exit code from the Chrome Cleaner process. Should be used only if
    // |launch_status| is |kSuccess|.
    int exit_code;

    ProcessStatus(LaunchStatus launch_status = LaunchStatus::kLaunchFailed,
                  int exit_code = std::numeric_limits<int>::max());
  };

  using ConnectionClosedCallback = base::OnceClosure;
  using ProcessDoneCallback = base::OnceCallback<void(ProcessStatus)>;

  // Executes the Chrome Cleaner in the background, initializes the IPC between
  // Chrome and the Chrome Cleaner process, and forwards IPC callbacks via the
  // callbacks that are passed to it.
  //
  // All callbacks are posted to provided |task_runner|.
  //
  // More details:
  //
  // This function will pass command line flags to the Chrome Cleaner
  // executable as appropriate based on the flags in |reporter_invocation| and
  // the |metrics_status| parameters. The Cleaner process will communicate with
  // Chrome via an IPC interface and any IPC requests or notifications are
  // passed to the caller via the |on_prompt_user| and |on_connection_closed|
  // callbacks. Finally, when the Chrome Cleaner process terminates, a
  // ProcessStatus is passed along to |on_process_done|.
  //
  // This IPC interface needs the |extension_service| in order to disable
  // extensions that the Cleaner process wants to disable.
  //
  // See ChromePromptChannel for more details of the IPC interface.
  static void RunChromeCleanerAndReplyWithExitCode(
      extensions::ExtensionService* extension_service,
      extensions::ExtensionRegistry* extension_registry,
      const base::FilePath& cleaner_executable_path,
      const SwReporterInvocation& reporter_invocation,
      ChromeMetricsStatus metrics_status,
      ChromePromptActions::PromptUserCallback on_prompt_user,
      ConnectionClosedCallback on_connection_closed,
      ProcessDoneCallback on_process_done,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Invokes the on_prompt_user_ callback, which should display a prompt to the
  // user and invoke |reply_callback| with the user's response. This is
  // intended to be called by ChromePromptChannel when a prompt request is
  // received over IPC.
  void OnPromptUser(
      ChromeCleanerScannerResults&& scanner_results,
      ChromePromptActions::PromptUserReplyCallback reply_callback);

  // Invokes the on_connection_closed_ callback, which should clean up after
  // the death of the cleaner. This is intended to be called by
  // ChromePromptChannel when the IPC connection is broken.
  void OnConnectionClosed();

 private:
  friend class base::RefCountedThreadSafe<ChromeCleanerRunner>;

  ~ChromeCleanerRunner();

  ChromeCleanerRunner(const base::FilePath& cleaner_executable_path,
                      const SwReporterInvocation& reporter_invocation,
                      ChromeMetricsStatus metrics_status,
                      ChromePromptActions::PromptUserCallback on_prompt_user,
                      ConnectionClosedCallback on_connection_closed,
                      ProcessDoneCallback on_process_done,
                      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ProcessStatus LaunchAndWaitForExitOnBackgroundThread(
      extensions::ExtensionService* extension_service,
      extensions::ExtensionRegistry* extension_registry);

  // Invokes the on_process_done_ callback, which should handle the results of
  // a full cleaner execution whose outcome is given by |launch_status|. This
  // is called with the result of LaunchAndWaitForExitOnBackgroundThread.
  void OnProcessDone(ProcessStatus launch_status);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::CommandLine cleaner_command_line_;
  ChromePromptActions::PromptUserCallback on_prompt_user_;
  ConnectionClosedCallback on_connection_closed_;
  ProcessDoneCallback on_process_done_;
};

// A delegate class used to override launching of the Cleaner proccess for
// tests.
class ChromeCleanerRunnerTestDelegate {
 public:
  virtual ~ChromeCleanerRunnerTestDelegate() = default;

  // Called instead of base::LaunchProcess() during testing.
  virtual base::Process LaunchTestProcess(
      const base::CommandLine& command_line,
      const base::LaunchOptions& launch_options) = 0;

  virtual void OnCleanerProcessDone(
      const ChromeCleanerRunner::ProcessStatus& process_status) = 0;
};

void SetChromeCleanerRunnerTestDelegateForTesting(
    ChromeCleanerRunnerTestDelegate* test_delegate);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_RUNNER_WIN_H_

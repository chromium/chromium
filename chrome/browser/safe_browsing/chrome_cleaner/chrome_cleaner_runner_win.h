// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_RUNNER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_RUNNER_WIN_H_

#include <limits>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_chrome_prompt_impl.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"

namespace safe_browsing {

// Class responsible for launching the cleaner process and waiting for its
// completion. This object is also responsible for starting the ChromePromptImpl
// object on the IO thread and controlling its lifetime.
//
// Expected lifecycle of a ChromeCleanerRunner:
//
//  - Instances are created via the static
//    RunChromeCleanerAndReplyWithExitCode() function. Instances will not be
//    destroyed before the Chrome Cleaner process has terminated _and_ a
//    Mojo connection-closed callback has been received. Destruction can happen
//    on any thread.
//  - The private LaunchAndWaitForExitOnBackgroundThread() function launches the
//    Chrome Cleaner process, creates a ChromePromptImpl object on the IO thread
//    and waits for the Cleaner process to terminate. The static
//    RunChromeCleanerAndReplyWithExitCode() function takes care of calling that
//    function correctly on a sequence with the correct traits.
//  - All callbacks registered with an instance are posted to run on the
//    provided |task_runner|
//  - The ChromePromptImpl object is destroyed on the IO thread after a
//    connection-closed has been received from Mojo. The runner instance will
//    not be destroyed before the ChromePromptImpl object has been released.
class ChromeCleanerRunner
    : public base::RefCountedThreadSafe<ChromeCleanerRunner> {
 public:
  enum class ChromeMetricsStatus {
    kEnabled,
    kDisabled,
  };

  enum class LaunchStatus {
    // Got an invalid process when attempting to launch the Chrome Cleaner
    // process. As a result, the Mojo pipe was never set up and the
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

  using ProcessDoneCallback = base::OnceCallback<void(ProcessStatus)>;

  // Executes the Chrome Cleaner in the background, initializes the Mojo IPC
  // between Chrome and the Chrome Cleaner process, and forwards Mojo callbacks
  // via the callbacks that are passed to it.
  //
  // All callbacks are posted to provided |task_runner|.
  //
  // More details:
  //
  // This function will pass command line flags to the Chrome Cleaner executable
  // as appropriate based on the flags in |reporter_invocation| and the
  // |metrics_status| parameters. The Cleaner process will communicate with
  // Chrome via a Mojo IPC interface and any IPC requests or notifications are
  // passed to the caller via the |on_prompt_user| and |on_connection_closed|
  // callbacks. Finally, when the Chrome Cleaner process terminates, a
  // ProcessStatus is passed along to |on_process_done|.
  //
  // This IPC interface needs the |extension_service| in order to
  // disable extensions that the Cleaner process wants to disable.
  //
  // The details of the mojo interface are documented in
  // "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h".
  static void RunChromeCleanerAndReplyWithExitCode(
      extensions::ExtensionService* extension_service,
      const base::FilePath& cleaner_executable_path,
      const SwReporterInvocation& reporter_invocation,
      ChromeMetricsStatus metrics_status,
      ChromePromptImpl::OnPromptUser on_prompt_user,
      base::OnceClosure on_connection_closed,
      ChromeCleanerRunner::ProcessDoneCallback on_process_done,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  friend class base::RefCountedThreadSafe<ChromeCleanerRunner>;

  ~ChromeCleanerRunner();

  ChromeCleanerRunner(extensions::ExtensionService* extension_service,
                      const base::FilePath& cleaner_executable_path,
                      const SwReporterInvocation& reporter_invocation,
                      ChromeMetricsStatus metrics_status,
                      ChromePromptImpl::OnPromptUser on_prompt_user,
                      base::OnceClosure on_connection_closed,
                      ChromeCleanerRunner::ProcessDoneCallback on_process_done,
                      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ProcessStatus LaunchAndWaitForExitOnBackgroundThread();

  void CreateChromePromptImpl(
      chrome_cleaner::mojom::ChromePromptRequest chrome_prompt_request);

  // Callbacks received from the Mojo interface.
  void OnPromptUser(ChromeCleanerScannerResults&& scanner_results,
                    chrome_cleaner::mojom::ChromePrompt::PromptUserCallback
                        prompt_user_callback);
  void OnConnectionClosed();
  void OnProcessDone(ProcessStatus launch_status);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::CommandLine cleaner_command_line_;
  ChromePromptImpl::OnPromptUser on_prompt_user_;
  base::OnceClosure on_connection_closed_;
  ProcessDoneCallback on_process_done_;

  std::unique_ptr<ChromePromptImpl, content::BrowserThread::DeleteOnIOThread>
      chrome_prompt_impl_;
  extensions::ExtensionService* extension_service_;
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

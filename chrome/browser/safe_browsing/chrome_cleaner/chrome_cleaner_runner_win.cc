// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"

#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/installer/util/install_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "extensions/browser/extension_system.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

using extensions::ExtensionService;
using chrome_cleaner::mojom::ChromePrompt;
using chrome_cleaner::mojom::ChromePromptRequest;
using content::BrowserThread;

namespace safe_browsing {

namespace {

// Global delegate used to override the launching of the Cleaner process during
// tests.
ChromeCleanerRunnerTestDelegate* g_test_delegate = nullptr;

}  // namespace

ChromeCleanerRunner::ProcessStatus::ProcessStatus(LaunchStatus launch_status,
                                                  int exit_code)
    : launch_status(launch_status), exit_code(exit_code) {}

// static
void ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(
    ExtensionService* extension_service,
    const base::FilePath& cleaner_executable_path,
    const SwReporterInvocation& reporter_invocation,
    ChromeMetricsStatus metrics_status,
    ChromePromptImpl::OnPromptUser on_prompt_user,
    base::OnceClosure on_connection_closed,
    ChromeCleanerRunner::ProcessDoneCallback on_process_done,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto cleaner_runner = base::WrapRefCounted(new ChromeCleanerRunner(
      extension_service, cleaner_executable_path, reporter_invocation,
      metrics_status, std::move(on_prompt_user),
      std::move(on_connection_closed), std::move(on_process_done),
      std::move(task_runner)));
  auto launch_and_wait = base::BindOnce(
      &ChromeCleanerRunner::LaunchAndWaitForExitOnBackgroundThread,
      cleaner_runner);
  auto process_done =
      base::BindOnce(&ChromeCleanerRunner::OnProcessDone, cleaner_runner);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      // LaunchAndWaitForExitOnBackgroundThread creates (MayBlock()) and joins
      // (WithBaseSyncPrimitives()) a process.
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(launch_and_wait), std::move(process_done));
}

ChromeCleanerRunner::ChromeCleanerRunner(
    ExtensionService* extension_service,
    const base::FilePath& cleaner_executable_path,
    const SwReporterInvocation& reporter_invocation,
    ChromeMetricsStatus metrics_status,
    ChromePromptImpl::OnPromptUser on_prompt_user,
    base::OnceClosure on_connection_closed,
    ProcessDoneCallback on_process_done,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      cleaner_command_line_(cleaner_executable_path),
      on_prompt_user_(std::move(on_prompt_user)),
      on_connection_closed_(std::move(on_connection_closed)),
      on_process_done_(std::move(on_process_done)),
      extension_service_(extension_service) {
  DCHECK(on_prompt_user_);
  DCHECK(on_connection_closed_);
  DCHECK(on_process_done_);
  DCHECK(!cleaner_executable_path.empty());

  // Add the non-IPC switches that should be passed to the Cleaner process.

  // Add switches that pass information about this Chrome installation.
  cleaner_command_line_.AppendSwitchASCII(chrome_cleaner::kChromeVersionSwitch,
                                          version_info::GetVersionNumber());
  cleaner_command_line_.AppendSwitchASCII(chrome_cleaner::kChromeChannelSwitch,
                                          base::IntToString(ChannelAsInt()));
  base::FilePath chrome_exe_path;
  base::PathService::Get(base::FILE_EXE, &chrome_exe_path);
  cleaner_command_line_.AppendSwitchPath(chrome_cleaner::kChromeExePathSwitch,
                                         chrome_exe_path);
  if (!InstallUtil::IsPerUserInstall())
    cleaner_command_line_.AppendSwitch(
        chrome_cleaner::kChromeSystemInstallSwitch);

  // Start the cleaner process in scanning mode.
  cleaner_command_line_.AppendSwitchASCII(
      chrome_cleaner::kExecutionModeSwitch,
      base::IntToString(
          static_cast<int>(chrome_cleaner::ExecutionMode::kScanning)));

  // If set, forward the engine flag from the reporter. Otherwise, set the
  // engine flag explicitly to 1.
  const std::string& reporter_engine =
      reporter_invocation.command_line().GetSwitchValueASCII(
          chrome_cleaner::kEngineSwitch);
  cleaner_command_line_.AppendSwitchASCII(
      chrome_cleaner::kEngineSwitch,
      reporter_engine.empty() ? "1" : reporter_engine);

  if (reporter_invocation.cleaner_logs_upload_enabled()) {
    cleaner_command_line_.AppendSwitch(
        chrome_cleaner::kWithScanningModeLogsSwitch);
  }

  cleaner_command_line_.AppendSwitchASCII(
      chrome_cleaner::kChromePromptSwitch,
      base::IntToString(static_cast<int>(reporter_invocation.chrome_prompt())));

  // If metrics is enabled, we can enable crash reporting in the Chrome Cleaner
  // process.
  if (metrics_status == ChromeMetricsStatus::kEnabled) {
    cleaner_command_line_.AppendSwitch(chrome_cleaner::kUmaUserSwitch);
    cleaner_command_line_.AppendSwitch(
        chrome_cleaner::kEnableCrashReportingSwitch);
  }

  const std::string group_name = GetSRTFieldTrialGroupName();
  if (!group_name.empty()) {
    cleaner_command_line_.AppendSwitchASCII(
        chrome_cleaner::kSRTPromptFieldTrialGroupNameSwitch, group_name);
  }

  std::string reboot_prompt_type = base::IntToString(GetRebootPromptType());
  cleaner_command_line_.AppendSwitchASCII(
      chrome_cleaner::kRebootPromptMethodSwitch, reboot_prompt_type);

  if (base::FeatureList::IsEnabled(kChromeCleanupQuarantineFeature)) {
    cleaner_command_line_.AppendSwitch(chrome_cleaner::kQuarantineSwitch);
  }
}

ChromeCleanerRunner::ProcessStatus
ChromeCleanerRunner::LaunchAndWaitForExitOnBackgroundThread() {
  TRACE_EVENT0("safe_browsing",
               "ChromeCleanerRunner::LaunchAndWaitForExitOnBackgroundThread");

  mojo::OutgoingInvitation invitation;
  std::string pipe_name = base::NumberToString(base::RandUint64());
  mojo::ScopedMessagePipeHandle request_pipe =
      invitation.AttachMessagePipe(pipe_name);
  cleaner_command_line_.AppendSwitchASCII(
      chrome_cleaner::kChromeMojoPipeTokenSwitch, pipe_name);

  mojo::PlatformChannel channel;
  base::LaunchOptions launch_options;
  channel.PrepareToPassRemoteEndpoint(&launch_options.handles_to_inherit,
                                      &cleaner_command_line_);

  base::Process cleaner_process =
      g_test_delegate
          ? g_test_delegate->LaunchTestProcess(cleaner_command_line_,
                                               launch_options)
          : base::LaunchProcess(cleaner_command_line_, launch_options);
  channel.RemoteProcessLaunchAttempted();
  if (!cleaner_process.IsValid())
    return ProcessStatus(LaunchStatus::kLaunchFailed);

  // ChromePromptImpl tasks will need to run on the IO thread. There is no
  // need to synchronize its creation, since the client end will wait for this
  // initialization to be done before sending requests.
  base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&ChromeCleanerRunner::CreateChromePromptImpl,
                                base::RetainedRef(this),
                                chrome_cleaner::mojom::ChromePromptRequest(
                                    std::move(request_pipe))));
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 cleaner_process.Handle(),
                                 channel.TakeLocalEndpoint());

  int exit_code = -1;
  if (!cleaner_process.WaitForExit(&exit_code)) {
    return ProcessStatus(
        LaunchStatus::kLaunchSucceededFailedToWaitForCompletion);
  }

  base::UmaHistogramSparse(
      "SoftwareReporter.Cleaner.ExitCodeFromConnectedProcess", exit_code);
  return ProcessStatus(LaunchStatus::kSuccess, exit_code);
}

ChromeCleanerRunner::~ChromeCleanerRunner() = default;

void ChromeCleanerRunner::CreateChromePromptImpl(
    ChromePromptRequest chrome_prompt_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!chrome_prompt_impl_);

  // Cannot use std::make_unique() since it does not support creating
  // std::unique_ptrs with custom deleters.
  chrome_prompt_impl_.reset(new ChromePromptImpl(
      extension_service_, std::move(chrome_prompt_request),
      base::Bind(&ChromeCleanerRunner::OnConnectionClosed,
                 base::RetainedRef(this)),
      base::Bind(&ChromeCleanerRunner::OnPromptUser, base::RetainedRef(this))));
}

void ChromeCleanerRunner::OnPromptUser(
    ChromeCleanerScannerResults&& scanner_results,
    ChromePrompt::PromptUserCallback prompt_user_callback) {
  if (on_prompt_user_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(on_prompt_user_),
                                          base::Passed(&scanner_results),
                                          base::Passed(&prompt_user_callback)));
  }
}

void ChromeCleanerRunner::OnConnectionClosed() {
  if (on_connection_closed_)
    task_runner_->PostTask(FROM_HERE, std::move(on_connection_closed_));
}

void ChromeCleanerRunner::OnProcessDone(ProcessStatus process_status) {
  if (g_test_delegate) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeCleanerRunnerTestDelegate::OnCleanerProcessDone,
                       base::Unretained(g_test_delegate), process_status));
  }

  if (on_process_done_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_process_done_), process_status));
  }
}

void SetChromeCleanerRunnerTestDelegateForTesting(
    ChromeCleanerRunnerTestDelegate* test_delegate) {
  g_test_delegate = test_delegate;
}

}  // namespace safe_browsing

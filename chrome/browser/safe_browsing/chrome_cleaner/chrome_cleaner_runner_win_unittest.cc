// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_process_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace safe_browsing {
namespace {

using ::chrome_cleaner::ChromePromptValue;
using ::chrome_cleaner::mojom::ChromePrompt;
using ::chrome_cleaner::mojom::PromptAcceptance;
using ::content::BrowserThread;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using ChromeMetricsStatus = ChromeCleanerRunner::ChromeMetricsStatus;

enum class ReporterEngine {
  kUnspecified,
  kOldEngine,
  kNewEngine,
};

// Simple test fixture that intercepts the launching of the Chrome Cleaner
// process and does not start a separate mock Cleaner process. It will pass an
// invalid process handle back to ChromeCleanerRunner. Intended for testing
// simple things like command line flags that Chrome sends to the Chrome Cleaner
// process.
//
// Parameters:
// - metrics_status (ChromeMetricsStatus): whether Chrome metrics reporting is
//       enabled
// - reporter_engine (ReporterEngine): the type of Cleaner engine specified in
//       the SwReporterInvocation.
// - cleaner_logs_enabled (bool): if logs can be collected in the cleaner
//       process running in scanning mode.
// - chrome_prompt (ChromePromptValue): indicates if this is a user-initiated
//       run or if the user was prompted.
// - quarantine_enabled (bool): indicates if the quarantine feature is enabled.
class ChromeCleanerRunnerSimpleTest
    : public testing::TestWithParam<
          std::tuple<ChromeCleanerRunner::ChromeMetricsStatus,
                     ReporterEngine,
                     bool,
                     ChromePromptValue,
                     bool>>,
      public ChromeCleanerRunnerTestDelegate {
 public:
  ChromeCleanerRunnerSimpleTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  void SetUp() override {
    std::tie(metrics_status_, reporter_engine_, cleaner_logs_enabled_,
             chrome_prompt_, quarantine_enabled_) = GetParam();

    std::vector<base::Feature> enabled_features;
    if (quarantine_enabled_) {
      enabled_features.push_back(kChromeCleanupQuarantineFeature);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});

    SetChromeCleanerRunnerTestDelegateForTesting(this);
  }

  void CallRunChromeCleaner() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    SwReporterInvocation reporter_invocation(command_line);
    switch (reporter_engine_) {
      case ReporterEngine::kUnspecified:
        // No engine switch.
        break;
      case ReporterEngine::kOldEngine:
        reporter_invocation.mutable_command_line().AppendSwitchASCII(
            chrome_cleaner::kEngineSwitch, "1");
        break;
      case ReporterEngine::kNewEngine:
        reporter_invocation.mutable_command_line().AppendSwitchASCII(
            chrome_cleaner::kEngineSwitch, "2");
        break;
    }

    reporter_invocation.set_cleaner_logs_upload_enabled(cleaner_logs_enabled_);

    reporter_invocation.set_chrome_prompt(chrome_prompt_);

    ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(
        /*extension_service=*/nullptr,
        base::FilePath(FILE_PATH_LITERAL("cleaner.exe")), reporter_invocation,
        metrics_status_,
        base::BindOnce(&ChromeCleanerRunnerSimpleTest::OnPromptUser,
                       base::Unretained(this)),
        base::BindOnce(&ChromeCleanerRunnerSimpleTest::OnConnectionClosed,
                       base::Unretained(this)),
        base::BindOnce(&ChromeCleanerRunnerSimpleTest::OnProcessDone,
                       base::Unretained(this)),
        base::ThreadTaskRunnerHandle::Get());
  }

  // ChromeCleanerRunnerTestDelegate overrides.

  base::Process LaunchTestProcess(
      const base::CommandLine& command_line,
      const base::LaunchOptions& launch_options) override {
    command_line_ = command_line;
    // Return an invalid process.
    return base::Process();
  }

  void OnCleanerProcessDone(
      const ChromeCleanerRunner::ProcessStatus& process_status) override {}

  // IPC callbacks.

  void OnPromptUser(ChromeCleanerScannerResults&& scanner_results,
                    ChromePrompt::PromptUserCallback response) {}

  void OnConnectionClosed() {}

  void OnProcessDone(ChromeCleanerRunner::ProcessStatus process_status) {
    on_process_done_called_ = true;
    process_status_ = process_status;
    run_loop_.QuitWhenIdle();
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  // Test fixture parameters.
  ChromeCleanerRunner::ChromeMetricsStatus metrics_status_;
  ReporterEngine reporter_engine_;
  bool cleaner_logs_enabled_ = false;
  ChromePromptValue chrome_prompt_ = ChromePromptValue::kUnspecified;
  bool quarantine_enabled_ = false;

  // Set by LaunchTestProcess.
  base::CommandLine command_line_;

  // Variables set by OnProcessDone().
  bool on_process_done_called_ = false;
  ChromeCleanerRunner::ProcessStatus process_status_;

  base::RunLoop run_loop_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ChromeCleanerRunnerSimpleTest, LaunchParams) {
  CallRunChromeCleaner();
  run_loop_.Run();

  EXPECT_TRUE(on_process_done_called_);

  EXPECT_EQ(
      command_line_.GetSwitchValueASCII(chrome_cleaner::kExecutionModeSwitch),
      base::IntToString(
          static_cast<int>(chrome_cleaner::ExecutionMode::kScanning)));

  // Ensure that the engine flag is always set and that it correctly reflects
  // the value of the same flag in the SwReporterInvocation() that was passed to
  // ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(). In the tests,
  // the engine flag in the SwReporterInvocation is controlled by the value of
  // |reporter_engine_|.
  //
  // If the engine switch is missing in reporter invocation, it should still be
  // explicitly set to the value "1" for the Cleaner.
  std::string expected_engine_switch =
      reporter_engine_ == ReporterEngine::kNewEngine ? "2" : "1";
  EXPECT_EQ(command_line_.GetSwitchValueASCII(chrome_cleaner::kEngineSwitch),
            expected_engine_switch);

  EXPECT_EQ(metrics_status_ == ChromeMetricsStatus::kEnabled,
            command_line_.HasSwitch(chrome_cleaner::kUmaUserSwitch));
  EXPECT_EQ(
      metrics_status_ == ChromeMetricsStatus::kEnabled,
      command_line_.HasSwitch(chrome_cleaner::kEnableCrashReportingSwitch));
  EXPECT_EQ(
      cleaner_logs_enabled_,
      command_line_.HasSwitch(chrome_cleaner::kWithScanningModeLogsSwitch));
  EXPECT_EQ(
      command_line_.GetSwitchValueASCII(chrome_cleaner::kChromePromptSwitch),
      base::IntToString(static_cast<int>(chrome_prompt_)));

  const std::string reboot_prompt_method = command_line_.GetSwitchValueASCII(
      chrome_cleaner::kRebootPromptMethodSwitch);
  int reboot_prompt = -1;
  EXPECT_TRUE(base::StringToInt(reboot_prompt_method, &reboot_prompt));

  EXPECT_EQ(quarantine_enabled_,
            command_line_.HasSwitch(chrome_cleaner::kQuarantineSwitch));
}

INSTANTIATE_TEST_CASE_P(
    All,
    ChromeCleanerRunnerSimpleTest,
    Combine(Values(ChromeCleanerRunner::ChromeMetricsStatus::kEnabled,
                   ChromeCleanerRunner::ChromeMetricsStatus::kDisabled),
            Values(ReporterEngine::kUnspecified,
                   ReporterEngine::kOldEngine,
                   ReporterEngine::kNewEngine),
            Bool(),
            Values(ChromePromptValue::kPrompted,
                   ChromePromptValue::kUserInitiated),
            Bool()));

// Enum to be used as parameter for the ChromeCleanerRunnerTest fixture below.
enum class UwsFoundState {
  kNoUwsFound,
  kUwsFoundRebootRequired,
  kUwsFoundNoRebootRequired,
};

// Test fixture for testing ChromeCleanerRunner with a mock Chrome Cleaner
// process.
//
// Parameters:
//
// - uws_found_state (UwsFoundState): Whether the Chrome Cleaner process should
//       find UwS on the system and if so whether reboot is required.
// - crash_point (CrashPoint): a single crash point where the Chrome Cleaner
//       process will crash. See the MockChromeCleanerProcess documentation for
//       possible values.
// - prompt_acceptance_to_send (PromptAcceptance): the prompt acceptance value
//       that Chrome should send to the Chrome Cleaner process. Must be DENIED
//       if |found_uws| is false.
class ChromeCleanerRunnerTest
    : public testing::TestWithParam<
          std::tuple<UwsFoundState,
                     MockChromeCleanerProcess::ItemsReporting,
                     MockChromeCleanerProcess::ItemsReporting,
                     MockChromeCleanerProcess::CrashPoint,
                     PromptAcceptance>>,
      public ChromeCleanerRunnerTestDelegate {
 public:
  ChromeCleanerRunnerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ChromeCleanerRunnerTest() override {}

  void SetUp() override {
    // Set up the testing profile, so chrome_cleaner_scanner_results can get the
    // extensions registry from it.
    ASSERT_TRUE(profile_manager_.SetUp());
    testing_profile_ = profile_manager_.CreateTestingProfile("Profile 1");
    MockChromeCleanerProcess::AddMockExtensionsToProfile(testing_profile_);

    UwsFoundState uws_found_state;
    MockChromeCleanerProcess::ItemsReporting registry_keys_reporting;
    MockChromeCleanerProcess::ItemsReporting extensions_reporting;
    MockChromeCleanerProcess::CrashPoint crash_point;
    PromptAcceptance prompt_acceptance_to_send;
    std::tie(uws_found_state, registry_keys_reporting, extensions_reporting,
             crash_point, prompt_acceptance_to_send) = GetParam();

    ASSERT_FALSE(uws_found_state == UwsFoundState::kNoUwsFound &&
                 prompt_acceptance_to_send != PromptAcceptance::DENIED);

    cleaner_process_options_.SetReportedResults(
        uws_found_state != UwsFoundState::kNoUwsFound, registry_keys_reporting,
        extensions_reporting);
    cleaner_process_options_.set_reboot_required(
        uws_found_state == UwsFoundState::kUwsFoundRebootRequired);
    cleaner_process_options_.set_crash_point(crash_point);
    cleaner_process_options_.set_expected_user_response(
        prompt_acceptance_to_send);
    prompt_acceptance_to_send_ = prompt_acceptance_to_send;

    SetChromeCleanerRunnerTestDelegateForTesting(this);
  }

  void CallRunChromeCleaner() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(
        /*extension_service=*/nullptr,
        base::FilePath(FILE_PATH_LITERAL("cleaner.exe")),
        SwReporterInvocation(command_line), ChromeMetricsStatus::kDisabled,
        base::BindOnce(&ChromeCleanerRunnerTest::OnPromptUser,
                       base::Unretained(this)),
        base::BindOnce(&ChromeCleanerRunnerTest::OnConnectionClosed,
                       base::Unretained(this)),
        base::BindOnce(&ChromeCleanerRunnerTest::OnProcessDone,
                       base::Unretained(this)),
        base::ThreadTaskRunnerHandle::Get());
  }

  // ChromeCleanerRunnerTestDelegate overrides.

  base::Process LaunchTestProcess(
      const base::CommandLine& command_line,
      const base::LaunchOptions& launch_options) override {
    // Add switches and program name that the test process needs for the multi
    // process tests.
    base::CommandLine test_process_command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    for (const auto& pair : command_line.GetSwitches())
      test_process_command_line.AppendSwitchNative(pair.first, pair.second);

    cleaner_process_options_.AddSwitchesToCommandLine(
        &test_process_command_line);

    base::Process process = base::SpawnMultiProcessTestChild(
        "MockChromeCleanerProcessMain", test_process_command_line,
        launch_options);

    EXPECT_TRUE(process.IsValid());
    return process;
  }

  void OnCleanerProcessDone(
      const ChromeCleanerRunner::ProcessStatus& process_status) override {}

  // IPC callbacks.

  // Will receive the main Mojo message from the Mock Chrome Cleaner process.
  void OnPromptUser(ChromeCleanerScannerResults&& scanner_results,
                    ChromePrompt::PromptUserCallback response) {
    on_prompt_user_called_ = true;
    received_scanner_results_ = std::move(scanner_results);
    base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
        ->PostTask(FROM_HERE, base::BindOnce(std::move(response),
                                             prompt_acceptance_to_send_));
  }

  void QuitTestRunLoopIfCommunicationDone() {
    if (on_process_done_called_ && on_connection_closed_called_)
      run_loop_.QuitWhenIdle();
  }

  void OnConnectionClosed() {
    on_connection_closed_called_ = true;
    QuitTestRunLoopIfCommunicationDone();
  }

  void OnProcessDone(ChromeCleanerRunner::ProcessStatus process_status) {
    on_process_done_called_ = true;
    process_status_ = process_status;
    QuitTestRunLoopIfCommunicationDone();
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* testing_profile_;

  base::RunLoop run_loop_;

  MockChromeCleanerProcess::Options cleaner_process_options_;
  PromptAcceptance prompt_acceptance_to_send_ = PromptAcceptance::UNSPECIFIED;

  // Set by OnProcessDone().
  ChromeCleanerRunner::ProcessStatus process_status_;

  // Set by OnPromptUser().
  ChromeCleanerScannerResults received_scanner_results_;

  bool on_prompt_user_called_ = false;
  bool on_connection_closed_called_ = false;
  bool on_process_done_called_ = false;
};

MULTIPROCESS_TEST_MAIN(MockChromeCleanerProcessMain) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  MockChromeCleanerProcess::Options options;
  EXPECT_TRUE(MockChromeCleanerProcess::Options::FromCommandLine(*command_line,
                                                                 &options));

  std::string chrome_mojo_pipe_token = command_line->GetSwitchValueASCII(
      chrome_cleaner::kChromeMojoPipeTokenSwitch);
  EXPECT_FALSE(chrome_mojo_pipe_token.empty());

  // Since failures in any of the above calls to EXPECT_*() do not actually fail
  // the test, we need to ensure that we return an exit code to indicate test
  // failure in such cases.
  if (::testing::Test::HasFailure())
    return MockChromeCleanerProcess::kInternalTestFailureExitCode;

  MockChromeCleanerProcess mock_cleaner_process(options,
                                                chrome_mojo_pipe_token);
  return mock_cleaner_process.Run();
}

TEST_P(ChromeCleanerRunnerTest, WithMockCleanerProcess) {
  CallRunChromeCleaner();
  run_loop_.Run();

  EXPECT_TRUE(on_process_done_called_);
  EXPECT_TRUE(on_connection_closed_called_);
  EXPECT_EQ(on_prompt_user_called_,
            (cleaner_process_options_.crash_point() ==
                 MockChromeCleanerProcess::CrashPoint::kNone ||
             cleaner_process_options_.crash_point() ==
                 MockChromeCleanerProcess::CrashPoint::kAfterResponseReceived));

  if (on_prompt_user_called_ &&
      !cleaner_process_options_.files_to_delete().empty()) {
    EXPECT_THAT(
        received_scanner_results_.files_to_delete(),
        UnorderedElementsAreArray(cleaner_process_options_.files_to_delete()));

    if (cleaner_process_options_.registry_keys()) {
      EXPECT_THAT(
          received_scanner_results_.registry_keys(),
          UnorderedElementsAreArray(*cleaner_process_options_.registry_keys()));
    } else {
      EXPECT_TRUE(received_scanner_results_.registry_keys().empty());
    }

    std::set<base::string16> extension_names;
    received_scanner_results_.FetchExtensionNames(testing_profile_,
                                                  &extension_names);
    if (cleaner_process_options_.extension_ids()) {
      EXPECT_THAT(extension_names,
                  UnorderedElementsAreArray(
                      *cleaner_process_options_.expected_extension_names()));
    } else {
      EXPECT_TRUE(extension_names.empty());
    }
  }

  EXPECT_EQ(process_status_.launch_status,
            ChromeCleanerRunner::LaunchStatus::kSuccess);
  EXPECT_EQ(
      process_status_.exit_code,
      cleaner_process_options_.ExpectedExitCode(prompt_acceptance_to_send_));
}

INSTANTIATE_TEST_CASE_P(
    All,
    ChromeCleanerRunnerTest,
    Combine(
        Values(UwsFoundState::kNoUwsFound),
        Values(MockChromeCleanerProcess::ItemsReporting::kUnsupported,
               MockChromeCleanerProcess::ItemsReporting::kNotReported,
               MockChromeCleanerProcess::ItemsReporting::kReported),
        Values(MockChromeCleanerProcess::ItemsReporting::kUnsupported,
               MockChromeCleanerProcess::ItemsReporting::kNotReported,
               MockChromeCleanerProcess::ItemsReporting::kReported),
        Values(MockChromeCleanerProcess::CrashPoint::kNone,
               MockChromeCleanerProcess::CrashPoint::kOnStartup,
               MockChromeCleanerProcess::CrashPoint::kAfterConnection,
               MockChromeCleanerProcess::CrashPoint::kAfterRequestSent,
               MockChromeCleanerProcess::CrashPoint::kAfterResponseReceived),
        Values(PromptAcceptance::DENIED)));

INSTANTIATE_TEST_CASE_P(
    UwsFound,
    ChromeCleanerRunnerTest,
    Combine(
        Values(UwsFoundState::kUwsFoundRebootRequired,
               UwsFoundState::kUwsFoundNoRebootRequired),
        Values(MockChromeCleanerProcess::ItemsReporting::kUnsupported,
               MockChromeCleanerProcess::ItemsReporting::kNotReported,
               MockChromeCleanerProcess::ItemsReporting::kReported),
        Values(MockChromeCleanerProcess::ItemsReporting::kUnsupported,
               MockChromeCleanerProcess::ItemsReporting::kNotReported,
               MockChromeCleanerProcess::ItemsReporting::kReported),
        Values(MockChromeCleanerProcess::CrashPoint::kNone,
               MockChromeCleanerProcess::CrashPoint::kOnStartup,
               MockChromeCleanerProcess::CrashPoint::kAfterConnection,
               MockChromeCleanerProcess::CrashPoint::kAfterRequestSent,
               MockChromeCleanerProcess::CrashPoint::kAfterResponseReceived),
        Values(PromptAcceptance::DENIED,
               PromptAcceptance::ACCEPTED_WITH_LOGS,
               PromptAcceptance::ACCEPTED_WITHOUT_LOGS)));

}  // namespace
}  // namespace safe_browsing

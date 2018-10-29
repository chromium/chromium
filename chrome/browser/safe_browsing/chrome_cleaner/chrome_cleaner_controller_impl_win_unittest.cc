// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_impl_win.h"

#include <sstream>
#include <string>
#include <tuple>
#include <utility>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/multiprocess_test.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_process_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace safe_browsing {
namespace {

using ::chrome_cleaner::mojom::PromptAcceptance;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using ::testing::_;
using CrashPoint = MockChromeCleanerProcess::CrashPoint;
using IdleReason = ChromeCleanerController::IdleReason;
using ItemsReporting = MockChromeCleanerProcess::ItemsReporting;
using State = ChromeCleanerController::State;
using UserResponse = ChromeCleanerController::UserResponse;

// Returns the PromptAcceptance value that ChromeCleanerController is supposed
// to send to the Chrome Cleaner process when ReplyWithUserResponse() is
// called with |user_response|.
PromptAcceptance UserResponseToPromptAcceptance(UserResponse user_response) {
  switch (user_response) {
    case UserResponse::kAcceptedWithLogs:
      return PromptAcceptance::ACCEPTED_WITH_LOGS;
    case UserResponse::kAcceptedWithoutLogs:
      return PromptAcceptance::ACCEPTED_WITHOUT_LOGS;
    case UserResponse::kDenied:  // Fallthrough
    case UserResponse::kDismissed:
      return PromptAcceptance::DENIED;
  }

  NOTREACHED();
  return PromptAcceptance::UNSPECIFIED;
}

class MockChromeCleanerControllerObserver
    : public ChromeCleanerController::Observer {
 public:
  MOCK_METHOD1(OnIdle, void(ChromeCleanerController::IdleReason));
  MOCK_METHOD0(OnReporterRunning, void());
  MOCK_METHOD0(OnScanning, void());
  MOCK_METHOD2(OnInfected, void(bool, const ChromeCleanerScannerResults&));
  MOCK_METHOD2(OnCleaning, void(bool, const ChromeCleanerScannerResults&));
  MOCK_METHOD0(OnRebootRequired, void());
  MOCK_METHOD0(OnRebootFailed, void());
  MOCK_METHOD1(OnLogsEnabledChanged, void(bool));
};

enum class MetricsStatus {
  kEnabled,
  kDisabled,
};

// Simple test fixture that passes an invalid process handle back to the
// ChromeCleanerRunner class and is intended for testing simple things like
// command line flags that Chrome sends to the Chrome Cleaner process.
//
// Parameters:
// - metrics_status (MetricsStatus): whether Chrome metrics reporting is
//   enabled.
class ChromeCleanerControllerSimpleTest
    : public testing::TestWithParam<MetricsStatus>,
      public ChromeCleanerRunnerTestDelegate,
      public ChromeCleanerControllerDelegate {
 public:
  ChromeCleanerControllerSimpleTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  ~ChromeCleanerControllerSimpleTest() override {}

  void SetUp() override {
    MetricsStatus metrics_status = GetParam();

    metrics_enabled_ = metrics_status == MetricsStatus::kEnabled;

    SetChromeCleanerRunnerTestDelegateForTesting(this);
    ChromeCleanerControllerImpl::ResetInstanceForTesting();
    ChromeCleanerControllerImpl::GetInstance()->SetDelegateForTesting(this);
  }

  void TearDown() override {
    ChromeCleanerControllerImpl::GetInstance()->SetDelegateForTesting(nullptr);
    SetChromeCleanerRunnerTestDelegateForTesting(nullptr);
    ChromeCleanerControllerImpl::ResetInstanceForTesting();
  }

  // ChromeCleanerControllerDelegate overrides.

  void FetchAndVerifyChromeCleaner(FetchedCallback fetched_callback) override {
    // In this fixture, we only test the cases when fetching the cleaner
    // executable succeeds.
    std::move(fetched_callback)
        .Run(base::FilePath(FILE_PATH_LITERAL("chrome_cleaner.exe")));
  }

  bool IsMetricsAndCrashReportingEnabled() override { return metrics_enabled_; }

  void TagForResetting(Profile* profile) override {
    // This function should never be called by these tests.
    FAIL();
  }

  void ResetTaggedProfiles(std::vector<Profile*> profiles,
                           base::OnceClosure continuation) override {
    // This function should never be called by these tests.
    FAIL();
  }

  void StartRebootPromptFlow(ChromeCleanerController* controller) override {
    FAIL();
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

 protected:
  // We need this because we need UI and IO threads during tests. The thread
  // bundle should be the first member of the class so that it will be destroyed
  // last.
  content::TestBrowserThreadBundle thread_bundle_;

  bool metrics_enabled_;
  base::CommandLine command_line_;

  StrictMock<MockChromeCleanerControllerObserver> mock_observer_;
};

SwReporterInvocation GetInvocationWithPromptTrigger() {
  return SwReporterInvocation(base::CommandLine(base::CommandLine::NO_PROGRAM))
      .WithSupportedBehaviours(SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT);
}

TEST_P(ChromeCleanerControllerSimpleTest, FlagsPassedToCleanerProcess) {
  ChromeCleanerControllerImpl* controller =
      ChromeCleanerControllerImpl::GetInstance();
  ASSERT_TRUE(controller);

  EXPECT_CALL(mock_observer_, OnIdle(_)).Times(1);
  controller->AddObserver(&mock_observer_);
  EXPECT_EQ(controller->state(), State::kIdle);

  EXPECT_CALL(mock_observer_, OnScanning()).Times(1);
  controller->Scan(GetInvocationWithPromptTrigger());
  EXPECT_EQ(controller->state(), State::kScanning);

  base::RunLoop run_loop;
  // The run loop will quit when we get back to the kIdle state, which will
  // happen when launching the Chrome Cleaner process fails (due to the
  // definition of LaunchTestProcess() in the test fixture class).
  EXPECT_CALL(mock_observer_, OnIdle(IdleReason::kScanningFailed))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.QuitWhenIdle(); }));
  run_loop.Run();

  EXPECT_EQ(controller->state(), State::kIdle);
  EXPECT_EQ(metrics_enabled_,
            command_line_.HasSwitch(chrome_cleaner::kUmaUserSwitch));
  EXPECT_EQ(metrics_enabled_, command_line_.HasSwitch(
                                  chrome_cleaner::kEnableCrashReportingSwitch));

  controller->RemoveObserver(&mock_observer_);
}

INSTANTIATE_TEST_CASE_P(All,
                        ChromeCleanerControllerSimpleTest,
                        Values(MetricsStatus::kDisabled,
                               MetricsStatus::kEnabled));

enum class CleanerProcessStatus {
  kFetchFailure,
  kFetchSuccessInvalidProcess,
  kFetchSuccessValidProcess,
};

enum class UwsFoundStatus {
  kNoUwsFound,
  kUwsFoundRebootRequired,
  kUwsFoundNoRebootRequired,
};

typedef std::tuple<CleanerProcessStatus,
                   CrashPoint,
                   UwsFoundStatus,
                   ItemsReporting,
                   ItemsReporting,
                   UserResponse>
    ChromeCleanerControllerTestParams;

// Test fixture that runs a mock Chrome Cleaner process in various
// configurations and mocks the user's response.
class ChromeCleanerControllerTest
    : public testing::WithParamInterface<ChromeCleanerControllerTestParams>,
      public ChromeCleanerRunnerTestDelegate,
      public ChromeCleanerControllerDelegate,
      public extensions::ExtensionServiceTestBase {
 public:
  ChromeCleanerControllerTest() = default;
  ~ChromeCleanerControllerTest() override {}

  void SetUp() override {
    InitializeEmptyExtensionService();
    std::tie(process_status_, crash_point_, uws_found_status_,
             registry_keys_reporting_, extensions_reporting_, user_response_) =
        GetParam();

    cleaner_process_options_.SetReportedResults(
        uws_found_status_ != UwsFoundStatus::kNoUwsFound,
        registry_keys_reporting_, extensions_reporting_);
    cleaner_process_options_.set_reboot_required(
        uws_found_status_ == UwsFoundStatus::kUwsFoundRebootRequired);
    cleaner_process_options_.set_crash_point(crash_point_);
    cleaner_process_options_.set_expected_user_response(
        uws_found_status_ == UwsFoundStatus::kNoUwsFound
            ? PromptAcceptance::DENIED
            : UserResponseToPromptAcceptance(user_response_));

    ChromeCleanerControllerImpl::ResetInstanceForTesting();
    controller_ = ChromeCleanerControllerImpl::GetInstance();
    ASSERT_TRUE(controller_);

    SetChromeCleanerRunnerTestDelegateForTesting(this);
    controller_->SetDelegateForTesting(this);
  }

  void TearDown() override {
    controller_->SetDelegateForTesting(nullptr);
    SetChromeCleanerRunnerTestDelegateForTesting(nullptr);
    ChromeCleanerControllerImpl::ResetInstanceForTesting();
  }

  // ChromeCleanerControllerDelegate overrides.

  void FetchAndVerifyChromeCleaner(FetchedCallback fetched_callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(fetched_callback),
            process_status_ != CleanerProcessStatus::kFetchFailure
                ? base::FilePath(FILE_PATH_LITERAL("chrome_cleaner.exe"))
                : base::FilePath()));
  }

  bool IsMetricsAndCrashReportingEnabled() override {
    // Returning an arbitrary value since this is not being tested in this
    // fixture.
    return false;
  }

  void TagForResetting(Profile* profile) override {
    profiles_tagged_.push_back(profile);
  }

  void ResetTaggedProfiles(std::vector<Profile*> profiles,
                           base::OnceClosure continuation) override {
    for (Profile* profile : profiles)
      profiles_to_reset_if_tagged_.push_back(profile);
    std::move(continuation).Run();
  }

  void StartRebootPromptFlow(ChromeCleanerController* controller) override {
    reboot_flow_started_ = true;
  }

  // ChromeCleanerRunnerTestDelegate overrides.

  base::Process LaunchTestProcess(
      const base::CommandLine& command_line,
      const base::LaunchOptions& launch_options) override {
    if (process_status_ != CleanerProcessStatus::kFetchSuccessValidProcess)
      return base::Process();

    // Add switches and program name that the test process needs for the multi
    // process tests.
    base::CommandLine test_process_command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    test_process_command_line.AppendArguments(command_line,
                                              /*include_program=*/false);

    cleaner_process_options_.AddSwitchesToCommandLine(
        &test_process_command_line);

    base::Process process = base::SpawnMultiProcessTestChild(
        "MockChromeCleanerProcessMain", test_process_command_line,
        launch_options);

    EXPECT_TRUE(process.IsValid());
    return process;
  }

  void OnCleanerProcessDone(
      const ChromeCleanerRunner::ProcessStatus& process_status) override {
    cleaner_process_status_ = process_status;
  }

  ChromeCleanerController::State ExpectedFinalState() {
    if (process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
        crash_point_ == CrashPoint::kNone &&
        uws_found_status_ == UwsFoundStatus::kUwsFoundRebootRequired &&
        ExpectedPromptAccepted()) {
      return State::kRebootRequired;
    }
    return State::kIdle;
  }

  bool ExpectedOnIdleCalled() { return ExpectedFinalState() == State::kIdle; }

  bool ExpectedOnInfectedCalled() {
    return process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
           crash_point_ != CrashPoint::kOnStartup &&
           crash_point_ != CrashPoint::kAfterConnection &&
           uws_found_status_ != UwsFoundStatus::kNoUwsFound;
  }

  bool ExpectedOnCleaningCalled() {
    return ExpectedOnInfectedCalled() &&
           crash_point_ != CrashPoint::kAfterRequestSent &&
           ExpectedPromptAccepted();
  }

  bool ExpectedOnRebootRequiredCalled() {
    return ExpectedFinalState() == State::kRebootRequired;
  }

  bool ExpectedUwsFound() { return ExpectedOnInfectedCalled(); }

  bool ExpectedRegistryKeysReported() {
    return ExpectedOnInfectedCalled() &&
           registry_keys_reporting_ == ItemsReporting::kReported;
  }

  bool ExpectedExtensionsReported() {
    return ExpectedOnInfectedCalled() &&
           extensions_reporting_ == ItemsReporting::kReported;
  }

  bool ExpectedPromptAccepted() {
    return user_response_ == UserResponse::kAcceptedWithLogs ||
           user_response_ == UserResponse::kAcceptedWithoutLogs;
  }

  bool ExpectedToTagProfile() {
    return process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
           (crash_point_ == CrashPoint::kNone ||
            crash_point_ == CrashPoint::kAfterResponseReceived) &&
           (uws_found_status_ == UwsFoundStatus::kUwsFoundNoRebootRequired ||
            uws_found_status_ == UwsFoundStatus::kUwsFoundRebootRequired) &&
           ExpectedPromptAccepted();
  }

  bool ExpectedToResetSettings() {
    return process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
           crash_point_ == CrashPoint::kNone &&
           uws_found_status_ == UwsFoundStatus::kUwsFoundNoRebootRequired &&
           ExpectedPromptAccepted();
  }

  ChromeCleanerController::IdleReason ExpectedIdleReason() {
    EXPECT_EQ(ExpectedFinalState(), State::kIdle);

    if (process_status_ == CleanerProcessStatus::kFetchFailure) {
      return IdleReason::kCleanerDownloadFailed;
    }

    if (process_status_ != CleanerProcessStatus::kFetchSuccessValidProcess ||
        crash_point_ == CrashPoint::kOnStartup ||
        crash_point_ == CrashPoint::kAfterConnection) {
      return IdleReason::kScanningFailed;
    }

    if (uws_found_status_ == UwsFoundStatus::kNoUwsFound)
      return IdleReason::kScanningFoundNothing;

    if (ExpectedOnInfectedCalled() &&
        (user_response_ == UserResponse::kDenied ||
         user_response_ == UserResponse::kDismissed)) {
      return IdleReason::kUserDeclinedCleanup;
    }

    if (ExpectedOnInfectedCalled() && ExpectedPromptAccepted() &&
        crash_point_ == CrashPoint::kAfterResponseReceived) {
      return IdleReason::kCleaningFailed;
    }

    return IdleReason::kCleaningSucceeded;
  }

  bool ExpectedRebootFlowStarted() {
    return process_status_ == CleanerProcessStatus::kFetchSuccessValidProcess &&
           crash_point_ == CrashPoint::kNone &&
           uws_found_status_ == UwsFoundStatus::kUwsFoundRebootRequired &&
           ExpectedPromptAccepted();
  }

 protected:
  CleanerProcessStatus process_status_;
  MockChromeCleanerProcess::CrashPoint crash_point_;
  UwsFoundStatus uws_found_status_;
  ItemsReporting registry_keys_reporting_;
  ItemsReporting extensions_reporting_;
  ChromeCleanerController::UserResponse user_response_;

  MockChromeCleanerProcess::Options cleaner_process_options_;

  StrictMock<MockChromeCleanerControllerObserver> mock_observer_;
  ChromeCleanerControllerImpl* controller_;
  ChromeCleanerRunner::ProcessStatus cleaner_process_status_;

  std::vector<Profile*> profiles_tagged_;
  std::vector<Profile*> profiles_to_reset_if_tagged_;

  bool reboot_flow_started_ = false;
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

TEST_P(ChromeCleanerControllerTest, WithMockCleanerProcess) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal(),
                                        &testing_local_state_);
  ASSERT_TRUE(profile_manager.SetUp());

  constexpr char kTestProfileName1[] = "Test 1";
  constexpr char kTestProfileName2[] = "Test 2";

  Profile* profile1 = profile_manager.CreateTestingProfile(kTestProfileName1);
  ASSERT_TRUE(profile1);
  Profile* profile2 = profile_manager.CreateTestingProfile(kTestProfileName2);
  ASSERT_TRUE(profile2);

  MockChromeCleanerProcess::AddMockExtensionsToProfile(profile1);

  const int num_profiles =
      profile_manager.profile_manager()->GetNumberOfProfiles();
  ASSERT_EQ(2, num_profiles);

  EXPECT_CALL(mock_observer_, OnIdle(_)).Times(1);
  controller_->AddObserver(&mock_observer_);
  EXPECT_EQ(controller_->state(), State::kIdle);

  EXPECT_CALL(mock_observer_, OnScanning()).Times(1);
  controller_->Scan(GetInvocationWithPromptTrigger());
  EXPECT_EQ(controller_->state(), State::kScanning);

  base::RunLoop run_loop;

  ChromeCleanerScannerResults scanner_results_on_infected;
  ChromeCleanerScannerResults scanner_results_on_cleaning;

  if (ExpectedOnIdleCalled()) {
    EXPECT_CALL(mock_observer_, OnIdle(ExpectedIdleReason()))
        .WillOnce(
            InvokeWithoutArgs([&run_loop]() { run_loop.QuitWhenIdle(); }));
  } else {
    EXPECT_CALL(mock_observer_, OnIdle(_)).Times(0);
  }

  if (ExpectedOnInfectedCalled()) {
    EXPECT_CALL(mock_observer_, OnInfected(_, _))
        .WillOnce(DoAll(SaveArg<1>(&scanner_results_on_infected),
                        InvokeWithoutArgs([this, profile1]() {
                          controller_->ReplyWithUserResponse(
                              profile1, service(), user_response_);
                        })));
    // Since logs upload is enabled by default, OnLogsEnabledChanged() will be
    // called only if the user response is kAcceptedWithoutLogs.
    if (user_response_ == UserResponse::kAcceptedWithoutLogs)
      EXPECT_CALL(mock_observer_, OnLogsEnabledChanged(false));
  } else {
    EXPECT_CALL(mock_observer_, OnInfected(_, _)).Times(0);
  }

  if (ExpectedOnCleaningCalled()) {
    EXPECT_CALL(mock_observer_, OnCleaning(_, _))
        .WillOnce(SaveArg<1>(&scanner_results_on_cleaning));
  } else {
    EXPECT_CALL(mock_observer_, OnCleaning(_, _)).Times(0);
  }

  if (ExpectedOnRebootRequiredCalled()) {
    EXPECT_CALL(mock_observer_, OnRebootRequired())
        .WillOnce(
            InvokeWithoutArgs([&run_loop]() { run_loop.QuitWhenIdle(); }));
  } else {
    EXPECT_CALL(mock_observer_, OnRebootRequired()).Times(0);
  }

  // Assert here that we expect at least one of OnIdle or OnRebootRequired to be
  // called, since otherwise, the test is set up incorrectly and is expected to
  // never stop.
  ASSERT_TRUE(ExpectedOnIdleCalled() || ExpectedOnRebootRequiredCalled());
  run_loop.Run();
  // Also ensure that we wait until the mock cleaner process has finished and
  // that all tasks that posted by ChromeCleanerRunner have run.
  content::RunAllTasksUntilIdle();

  EXPECT_NE(cleaner_process_status_.exit_code,
            MockChromeCleanerProcess::kInternalTestFailureExitCode);
  EXPECT_EQ(controller_->state(), ExpectedFinalState());

  EXPECT_EQ(!scanner_results_on_infected.files_to_delete().empty(),
            ExpectedUwsFound());
  EXPECT_EQ(!scanner_results_on_cleaning.files_to_delete().empty(),
            ExpectedUwsFound() && ExpectedOnCleaningCalled());
  if (!scanner_results_on_cleaning.files_to_delete().empty()) {
    EXPECT_THAT(scanner_results_on_cleaning.files_to_delete(),
                UnorderedElementsAreArray(
                    scanner_results_on_infected.files_to_delete()));
  }

  EXPECT_EQ(!scanner_results_on_infected.registry_keys().empty(),
            ExpectedRegistryKeysReported());
  EXPECT_EQ(!scanner_results_on_cleaning.registry_keys().empty(),
            ExpectedRegistryKeysReported() && ExpectedOnCleaningCalled());
  if (!scanner_results_on_cleaning.registry_keys().empty()) {
    EXPECT_THAT(
        scanner_results_on_cleaning.registry_keys(),
        UnorderedElementsAreArray(scanner_results_on_infected.registry_keys()));
  }

  std::set<base::string16> extension_names_infected;
  scanner_results_on_infected.FetchExtensionNames(profile1,
                                                  &extension_names_infected);
  std::set<base::string16> extension_names_cleaning;
  scanner_results_on_cleaning.FetchExtensionNames(profile1,
                                                  &extension_names_cleaning);
// Extension names only reported on Windows Chrome build.
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  EXPECT_EQ(!extension_names_infected.empty(), ExpectedExtensionsReported());
  EXPECT_EQ(!extension_names_cleaning.empty(),
            ExpectedExtensionsReported() && ExpectedOnCleaningCalled());
  if (!extension_names_cleaning.empty()) {
    EXPECT_THAT(extension_names_cleaning,
                UnorderedElementsAreArray(extension_names_infected));
  }
#else
  EXPECT_TRUE(extension_names_infected.empty());
  EXPECT_TRUE(extension_names_cleaning.empty());
#endif

  EXPECT_EQ(ExpectedRebootFlowStarted(), reboot_flow_started_);

  std::vector<Profile*> expected_tagged;
  if (ExpectedToTagProfile())
    expected_tagged.push_back(profile1);
  EXPECT_THAT(expected_tagged, UnorderedElementsAreArray(profiles_tagged_));

  std::vector<Profile*> expected_reset_if_tagged;
  if (ExpectedToResetSettings()) {
    expected_reset_if_tagged.push_back(profile1);
    expected_reset_if_tagged.push_back(profile2);
  }
  EXPECT_THAT(expected_reset_if_tagged,
              UnorderedElementsAreArray(profiles_to_reset_if_tagged_));

  controller_->RemoveObserver(&mock_observer_);
}

// Make all the test parameter types printable.

std::ostream& operator<<(std::ostream& out, CleanerProcessStatus status) {
  switch (status) {
    case CleanerProcessStatus::kFetchFailure:
      return out << "FetchFailure";
    case CleanerProcessStatus::kFetchSuccessInvalidProcess:
      return out << "FetchSuccessInvalidProcess";
    case CleanerProcessStatus::kFetchSuccessValidProcess:
      return out << "FetchSuccessValidProcess";
    default:
      NOTREACHED();
      return out << "UnknownProcessStatus" << status;
  }
}

std::ostream& operator<<(std::ostream& out, CrashPoint crash_point) {
  switch (crash_point) {
    case CrashPoint::kNone:
      return out << "NoCrash";
    case CrashPoint::kOnStartup:
      return out << "CrashOnStartup";
    case CrashPoint::kAfterConnection:
      return out << "CrashAfterConnection";
    case CrashPoint::kAfterRequestSent:
      return out << "CrashAfterRequestSent";
    case CrashPoint::kAfterResponseReceived:
      return out << "CrashAfterResponseReceived";
    default:
      NOTREACHED();
      return out << "UnknownCrashPoint" << crash_point;
  }
}

std::ostream& operator<<(std::ostream& out, UwsFoundStatus status) {
  switch (status) {
    case UwsFoundStatus::kNoUwsFound:
      return out << "NoUwsFound";
    case UwsFoundStatus::kUwsFoundRebootRequired:
      return out << "UwsFoundRebootRequired";
    case UwsFoundStatus::kUwsFoundNoRebootRequired:
      return out << "UwsFoundNoRebootRequired";
    default:
      NOTREACHED();
      return out << "UnknownFoundStatus" << status;
  }
}

std::ostream& operator<<(std::ostream& out, ItemsReporting items_reporting) {
  switch (items_reporting) {
    case ItemsReporting::kUnsupported:
      return out << "kUnsupported";
    case ItemsReporting::kNotReported:
      return out << "kNotReported";
    case ItemsReporting::kReported:
      return out << "kReported";
    default:
      NOTREACHED();
      return out << "UnknownItemsReporting";
  }
}

std::ostream& operator<<(std::ostream& out, UserResponse response) {
  switch (response) {
    case UserResponse::kAcceptedWithLogs:
      return out << "UserAcceptedWithLogs";
    case UserResponse::kAcceptedWithoutLogs:
      return out << "UserAcceptedWithoutLogs";
    case UserResponse::kDenied:
      return out << "UserDenied";
    case UserResponse::kDismissed:
      return out << "UserDismissed";
    default:
      NOTREACHED();
      return out << "UnknownUserResponse" << response;
  }
}

// ::testing::PrintToStringParamName does not format tuples as a valid test
// name, so this functor can be used to get each element in the tuple
// explicitly and format them using the above operator<< overrides.
struct ChromeCleanerControllerTestParamsToString {
  std::string operator()(
      const ::testing::TestParamInfo<ChromeCleanerControllerTestParams>& info)
      const {
    std::ostringstream param_name;
    param_name << std::get<0>(info.param) << "_";
    param_name << std::get<1>(info.param) << "_";
    param_name << std::get<2>(info.param) << "_";
    param_name << std::get<3>(info.param) << "_";
    param_name << std::get<4>(info.param) << "_";
    param_name << std::get<5>(info.param);
    return param_name.str();
  }
};

INSTANTIATE_TEST_CASE_P(
    All,
    ChromeCleanerControllerTest,
    Combine(Values(CleanerProcessStatus::kFetchFailure,
                   CleanerProcessStatus::kFetchSuccessInvalidProcess,
                   CleanerProcessStatus::kFetchSuccessValidProcess),
            Values(CrashPoint::kNone,
                   CrashPoint::kOnStartup,
                   CrashPoint::kAfterConnection,
                   // CrashPoint::kAfterRequestSent is not used because we
                   // cannot ensure the order between the Mojo request being
                   // received by Chrome and the connection being lost.
                   CrashPoint::kAfterResponseReceived),
            Values(UwsFoundStatus::kNoUwsFound,
                   UwsFoundStatus::kUwsFoundRebootRequired,
                   UwsFoundStatus::kUwsFoundNoRebootRequired),
            Values(ItemsReporting::kUnsupported,
                   ItemsReporting::kNotReported,
                   ItemsReporting::kReported),
            Values(ItemsReporting::kUnsupported,
                   ItemsReporting::kNotReported,
                   ItemsReporting::kReported),
            Values(UserResponse::kAcceptedWithLogs,
                   UserResponse::kAcceptedWithoutLogs,
                   UserResponse::kDenied,
                   UserResponse::kDismissed)),
    ChromeCleanerControllerTestParamsToString());

// Tests for the interaction between reporter runs and all possible states.
// Signals from reporter execution may lead to state transitions only if there
// is no cleaner activity, so it's enough to check the state after a signal.
//
// Parameters:
//  - initial_state_: the state of the controller before receiving a signal from
//        the reporter.
using ChromeCleanerControllerReporterInteractionTestParams =
    ChromeCleanerController::State;

class ChromeCleanerControllerReporterInteractionTest
    : public testing::TestWithParam<
          ChromeCleanerControllerReporterInteractionTestParams>,
      public ChromeCleanerControllerDelegate {
 public:
  void SetUp() override {
    initial_state_ = GetParam();

    ChromeCleanerControllerImpl::ResetInstanceForTesting();
    controller_ = ChromeCleanerControllerImpl::GetInstance();
    ASSERT_TRUE(controller_);

    controller_->SetDelegateForTesting(this);

    controller_->SetStateForTesting(initial_state_);
    ASSERT_EQ(initial_state_, controller_->state());
  }

  void TearDown() override {
    controller_->SetDelegateForTesting(nullptr);
    ChromeCleanerControllerImpl::ResetInstanceForTesting();
  }

  void ExpectNoStateChangeOnReporterSequenceDone(
      SwReporterInvocationResult reporter_result) {
    controller_->OnReporterSequenceDone(reporter_result);
    EXPECT_EQ(initial_state_, controller_->state());
  }

  void MaybeExpectStateChangeToIdle(
      SwReporterInvocationResult reporter_result,
      ChromeCleanerController::IdleReason idle_reason) {
    controller_->OnReporterSequenceDone(reporter_result);
    if (initial_state_ == ChromeCleanerController::State::kReporterRunning) {
      EXPECT_EQ(ChromeCleanerController::State::kIdle, controller_->state());
      EXPECT_EQ(idle_reason, controller_->idle_reason());
    } else {
      EXPECT_EQ(initial_state_, controller_->state());
    }
  }

  // We need this because we need UI and IO threads during tests. The thread
  // bundle should be the first member of the class so that it will be destroyed
  // last.
  content::TestBrowserThreadBundle thread_bundle_;

  ChromeCleanerController::State initial_state_;

  ChromeCleanerControllerImpl* controller_ = nullptr;
  StrictMock<MockChromeCleanerControllerObserver> mock_observer_;
};

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceStarted) {
  controller_->OnReporterSequenceStarted();
  EXPECT_EQ(initial_state_ == ChromeCleanerController::State::kIdle
                ? ChromeCleanerController::State::kReporterRunning
                : initial_state_,
            controller_->state());
}

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceDone_NotScheduled) {
  ExpectNoStateChangeOnReporterSequenceDone(
      SwReporterInvocationResult::kNotScheduled);
}

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceDone_TimedOut) {
  MaybeExpectStateChangeToIdle(
      SwReporterInvocationResult::kTimedOut,
      ChromeCleanerController::IdleReason::kReporterFailed);
}

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceDone_ProcessFailedToLaunch) {
  MaybeExpectStateChangeToIdle(
      SwReporterInvocationResult::kProcessFailedToLaunch,
      ChromeCleanerController::IdleReason::kReporterFailed);
}

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceDone_GeneralFailure) {
  MaybeExpectStateChangeToIdle(
      SwReporterInvocationResult::kGeneralFailure,
      ChromeCleanerController::IdleReason::kReporterFailed);
}

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceDone_NothingFound) {
  MaybeExpectStateChangeToIdle(
      SwReporterInvocationResult::kNothingFound,
      ChromeCleanerController::IdleReason::kReporterFoundNothing);
}

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceDone_CleanupNotOffered) {
  MaybeExpectStateChangeToIdle(
      SwReporterInvocationResult::kCleanupNotOffered,
      ChromeCleanerController::IdleReason::kReporterFoundNothing);
}

TEST_P(ChromeCleanerControllerReporterInteractionTest,
       OnReporterSequenceDone_CleanupToBeOffered) {
  ExpectNoStateChangeOnReporterSequenceDone(
      SwReporterInvocationResult::kCleanupToBeOffered);
}

std::ostream& operator<<(std::ostream& out,
                         ChromeCleanerController::State state) {
  switch (state) {
    case ChromeCleanerController::State::kIdle:
      return out << "Idle";
    case ChromeCleanerController::State::kReporterRunning:
      return out << "ReporterRunning";
    case ChromeCleanerController::State::kScanning:
      return out << "Scanning";
    case ChromeCleanerController::State::kInfected:
      return out << "Infected";
    case ChromeCleanerController::State::kCleaning:
      return out << "Cleaning";
    case ChromeCleanerController::State::kRebootRequired:
      return out << "RebootRequired";
    default:
      NOTREACHED();
      return out << "UnknownUserResponse" << state;
  }
}

INSTANTIATE_TEST_CASE_P(
    All,
    ChromeCleanerControllerReporterInteractionTest,
    Values(ChromeCleanerController::State::kIdle,
           ChromeCleanerController::State::kReporterRunning,
           ChromeCleanerController::State::kScanning,
           ChromeCleanerController::State::kInfected,
           ChromeCleanerController::State::kCleaning,
           ChromeCleanerController::State::kRebootRequired));

}  // namespace
}  // namespace safe_browsing

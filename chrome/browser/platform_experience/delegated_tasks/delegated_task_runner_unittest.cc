// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_switches.h"
#include "chrome/browser/platform_experience/delegated_tasks/test_support/mock_peh_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using ::testing::_;
using ::testing::Return;

namespace platform_experience {

namespace {

constexpr int kTaskSuccessExitCode = 0;
constexpr int kTaskTimeoutSeconds = 5;

constexpr char kTaskName[] = "TestTask";
constexpr char kTaskCustomSwitchKey[] = "post-install-url";
constexpr char kTaskCustomSwitchValue[] = "https://example.com";

const base::FilePath::CharType kFakeBinaryPath[] =
    FILE_PATH_LITERAL("C:\\path\\to\\fake_binary.exe");

class TestDelegatedTask : public DelegatedTask {
 public:
  TestDelegatedTask() = default;
  ~TestDelegatedTask() override = default;

  DelegatedTaskType GetTaskType() const override {
    return DelegatedTaskType::kRegisterSearchPromotion;
  }
  base::TimeDelta GetTimeout() const override {
    return base::Seconds(kTaskTimeoutSeconds);
  }

  void AppendCommandLineSwitches(base::CommandLine& cmd_line) const override {
    cmd_line.AppendSwitchASCII(kTaskCustomSwitchKey, kTaskCustomSwitchValue);
  }

  std::string_view GetTaskName() const override { return kTaskName; }
};

class DelegatedTaskRunnerTest : public base::MultiProcessTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

class DelegatedTaskRunnerMockTimeTest : public base::MultiProcessTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

MULTIPROCESS_TEST_MAIN(SuccessProcess) {
  return kTaskSuccessExitCode;
}

MULTIPROCESS_TEST_MAIN(InvalidTaskProcess) {
  return static_cast<int>(PehExitCode::kInvalidTaskType);
}

MULTIPROCESS_TEST_MAIN(InvalidArgsProcess) {
  return static_cast<int>(PehExitCode::kInvalidArgs);
}

MULTIPROCESS_TEST_MAIN(TimeoutProcess) {
  // Sleep for longer than the task timeout.
  base::PlatformThread::Sleep(base::Seconds(kTaskTimeoutSeconds * 2));
  return kTaskSuccessExitCode;
}

MULTIPROCESS_TEST_MAIN(CustomExitCodeProcess) {
  return 42;
}

std::unique_ptr<testing::NiceMock<MockPehLauncher>>
CreateDefaultMockLauncher() {
  auto mock_launcher = std::make_unique<testing::NiceMock<MockPehLauncher>>();
  ON_CALL(*mock_launcher, GetBinaryPath())
      .WillByDefault(Return(base::FilePath(kFakeBinaryPath)));
  ON_CALL(*mock_launcher, GetBinaryVersion(_))
      .WillByDefault(Return(base::Version("1.0.0.0")));
  return mock_launcher;
}

}  // namespace

TEST_F(DelegatedTaskRunnerTest, BinaryNotFound) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _)).Times(0);

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kPehNotFound);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kPehNotFound, 1);
  histogram_tester.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Duration", 1);
}

TEST_F(DelegatedTaskRunnerTest, ProcessLaunchFailure) {
  auto mock_launcher = CreateDefaultMockLauncher();

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce(Return(base::Process()));

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kProcessLaunchFailure);
}

TEST_F(DelegatedTaskRunnerTest, PehValidationFailure) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  EXPECT_CALL(*mock_launcher, GetBinaryPath())
      .WillOnce(Return(base::FilePath(kFakeBinaryPath)));
  EXPECT_CALL(*mock_launcher, IsBinaryVerified(_)).WillOnce(Return(false));
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _)).Times(0);

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kPehValidationFailure);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kPehValidationFailure, 1);
  histogram_tester.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Duration", 1);
}

TEST_F(DelegatedTaskRunnerTest, SuccessAndCommandLineVerification) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = CreateDefaultMockLauncher();

  base::CommandLine launched_cmd_line(base::CommandLine::NO_PROGRAM);
  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        launched_cmd_line = cmd_line;
        return base::SpawnMultiProcessTestChild(
            "SuccessProcess", base::GetMultiProcessTestChildBaseCommandLine(),
            options);
      });

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.value(), kTaskSuccessExitCode);

  EXPECT_EQ(launched_cmd_line.GetProgram(), base::FilePath(kFakeBinaryPath));
  EXPECT_EQ(launched_cmd_line.GetSwitchValueASCII(kDelegatedTasksSwitch),
            kTaskName);

  EXPECT_EQ(launched_cmd_line.GetSwitchValueASCII(kTaskCustomSwitchKey),
            kTaskCustomSwitchValue);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Duration", 1);
}

TEST_F(DelegatedTaskRunnerTest, InvalidTask) {
  auto mock_launcher = CreateDefaultMockLauncher();

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "InvalidTaskProcess",
            base::GetMultiProcessTestChildBaseCommandLine(), options);
      });

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kInvalidTaskType);
}

TEST_F(DelegatedTaskRunnerTest, InvalidArgs) {
  auto mock_launcher = CreateDefaultMockLauncher();

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "InvalidArgsProcess",
            base::GetMultiProcessTestChildBaseCommandLine(), options);
      });

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kInvalidArgs);
}

TEST_F(DelegatedTaskRunnerTest, InvalidMinVersion) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = std::make_unique<MockPehLauncher>();
  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));

  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  // Passing an empty or invalid version string fails immediately.
  runner->Run(std::move(task), "", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kUnsupportedVersion);
}

TEST_F(DelegatedTaskRunnerTest, CustomExitCodeLogsSuccess) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = CreateDefaultMockLauncher();

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "CustomExitCodeProcess",
            base::GetMultiProcessTestChildBaseCommandLine(), options);
      });

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.value(), 42);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Duration", 1);
}

TEST_F(DelegatedTaskRunnerMockTimeTest, Timeout) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = CreateDefaultMockLauncher();

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "TimeoutProcess", base::GetMultiProcessTestChildBaseCommandLine(),
            options);
      });

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  // Fast-forward mock time to trigger the timeout.
  task_environment_.FastForwardBy(base::Seconds(kTaskTimeoutSeconds + 1));

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kTaskTimeout);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kTaskTimeout, 1);
  histogram_tester.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Duration", 1);
}

TEST_F(DelegatedTaskRunnerTest, RunnerDestroyedBeforeTaskCompletion) {
  auto mock_launcher = CreateDefaultMockLauncher();

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());
  runner.reset();

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kRunnerDestroyedBeforeTaskCompletion);
}

TEST_F(DelegatedTaskRunnerTest, RunnerDestroyedWhileProcessLaunchInFlight) {
  auto mock_launcher = CreateDefaultMockLauncher();

  base::RunLoop run_loop;

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([quit_closure = run_loop.QuitClosure()](
                    const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        quit_closure.Run();
        return base::Process();
      });

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "0.0.0.0", future.GetCallback());

  // Process main thread tasks until LaunchProcess is invoked.
  run_loop.Run();

  // Destroy the runner while LaunchProcess completion is in-flight.
  runner.reset();

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kRunnerDestroyedBeforeTaskCompletion);
}

TEST_F(DelegatedTaskRunnerTest, UnsupportedVersion) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = CreateDefaultMockLauncher();

  EXPECT_CALL(*mock_launcher, GetBinaryVersion(_))
      .WillOnce(Return(base::Version("151.0.0.0")));

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "152.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kUnsupportedVersion);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kUnsupportedVersion, 1);
  histogram_tester.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Duration", 1);
}

TEST_F(DelegatedTaskRunnerTest, SupportedVersion) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = CreateDefaultMockLauncher();

  EXPECT_CALL(*mock_launcher, GetBinaryVersion(_))
      .WillOnce(Return(base::Version("153.0.0.0")));

  EXPECT_CALL(*mock_launcher, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd_line,
                    const base::LaunchOptions& options) {
        return base::SpawnMultiProcessTestChild(
            "SuccessProcess", base::GetMultiProcessTestChildBaseCommandLine(),
            options);
      });

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "152.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.value(), kTaskSuccessExitCode);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Duration", 1);
}

TEST_F(DelegatedTaskRunnerTest, InvalidBinaryVersion) {
  base::HistogramTester histogram_tester;
  auto mock_launcher = CreateDefaultMockLauncher();

  // Return an invalid version (e.g. if the file lacks VERSIONINFO).
  EXPECT_CALL(*mock_launcher, GetBinaryVersion(_))
      .WillOnce(Return(base::Version()));

  auto runner = std::make_unique<DelegatedTaskRunner>(std::move(mock_launcher));
  auto task = std::make_unique<TestDelegatedTask>();
  base::test::TestFuture<DelegatedTaskResult> future;

  runner->Run(std::move(task), "152.0.0.0", future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.exit_code_or_status.has_value());
  EXPECT_EQ(result.exit_code_or_status.error(),
            DelegatedTaskStatus::kUnsupportedVersion);

  histogram_tester.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.DelegatedTasks.TestTask.Status",
      DelegatedTaskStatus::kUnsupportedVersion, 1);
}

}  // namespace platform_experience

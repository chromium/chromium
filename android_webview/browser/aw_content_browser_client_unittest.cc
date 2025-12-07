// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include <memory>
#include <vector>

#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/common/aw_switches.h"
#include "base/android/yield_to_looper_checker.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

using ::testing::InSequence;
using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;
using base::android::YieldToLooperChecker;

enum class StartupTaskExperiment {
  kNone,
  kUseStartupTasksLogic,
  kUseStartupTasksLogicP2,
  kStartupTasksYieldToNative,
};

std::string StartupTaskExperimentToString(
    const ::testing::TestParamInfo<StartupTaskExperiment>& info) {
  switch (info.param) {
    case StartupTaskExperiment::kNone:
      return "NoExperiment";
    case StartupTaskExperiment::kUseStartupTasksLogic:
      return "UseStartupTasksLogic";
    case StartupTaskExperiment::kUseStartupTasksLogicP2:
      return "UseStartupTasksLogicP2";
    case StartupTaskExperiment::kStartupTasksYieldToNative:
      return "StartupTasksYieldToNative";
  }
}

class AwContentBrowserClientTest
    : public testing::TestWithParam<StartupTaskExperiment> {
 public:
  AwContentBrowserClientTest() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    switch (GetParam()) {
      case StartupTaskExperiment::kNone:
        break;
      case StartupTaskExperiment::kUseStartupTasksLogic:
        command_line->AppendSwitch(switches::kWebViewUseStartupTasksLogic);
        break;
      case StartupTaskExperiment::kUseStartupTasksLogicP2:
        command_line->AppendSwitch(switches::kWebViewUseStartupTasksLogicP2);
        break;
      case StartupTaskExperiment::kStartupTasksYieldToNative:
        command_line->AppendSwitch(switches::kWebViewStartupTasksYieldToNative);
        break;
      default:
        CHECK(false) << "Unhandled experiment";
    }
  }

  bool IsAnyExperimentEnabled() {
    return GetParam() != StartupTaskExperiment::kNone;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AwContentBrowserClient client_{
      std::make_unique<AwFeatureListCreator>().get()};
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
};

TEST_P(AwContentBrowserClientTest, ClientTaskNotRunBeforeStartupComplete) {
  StrictMockTask client_task;

  if (IsAnyExperimentEnabled()) {
    StrictMockTask loop_quitting_task;

    client_.PostAfterStartupTask(FROM_HERE, task_runner_, client_task.Get());

    // Run loop to confirm that client task is not executed.
    base::RunLoop run_loop;
    EXPECT_CALL(loop_quitting_task, Run)
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    task_runner_->PostTask(FROM_HERE, loop_quitting_task.Get());
    run_loop.Run();

    base::RunLoop task_run_loop;
    EXPECT_CALL(client_task, Run)
        .WillOnce(base::test::RunOnceClosure(task_run_loop.QuitClosure()));
    client_.OnStartupComplete();
    task_run_loop.Run();
  } else {
    base::RunLoop run_loop;
    EXPECT_CALL(client_task, Run)
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    client_.PostAfterStartupTask(FROM_HERE, task_runner_, client_task.Get());
    run_loop.Run();
  }
}

TEST_P(AwContentBrowserClientTest, TaskRunAfterStartupComplete) {
  StrictMockTask task;

  // Task should run without startup complete call if no experiment
  if (IsAnyExperimentEnabled()) {
    client_.OnStartupComplete();
  }

  base::RunLoop run_loop;
  EXPECT_CALL(task, Run).WillOnce(
      base::test::RunOnceClosure(run_loop.QuitClosure()));
  client_.PostAfterStartupTask(FROM_HERE, task_runner_, task.Get());

  run_loop.Run();
}

TEST_P(AwContentBrowserClientTest, MultipleTasksBeforeStartup) {
  StrictMockTask task1;
  StrictMockTask task2;
  StrictMockTask task3;

  base::RunLoop run_loop;
  auto setup_call_expectations = [&] {
    InSequence s;
    EXPECT_CALL(task1, Run);
    EXPECT_CALL(task2, Run);
    EXPECT_CALL(task3, Run)
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
  };

  auto post_after_startup_tasks = [&] {
    client_.PostAfterStartupTask(FROM_HERE, task_runner_, task1.Get());
    client_.PostAfterStartupTask(FROM_HERE, task_runner_, task2.Get());
    client_.PostAfterStartupTask(FROM_HERE, task_runner_, task3.Get());
  };

  if (IsAnyExperimentEnabled()) {
    // AfterStartupTasks only running after startup is marked as complete.
    post_after_startup_tasks();
    setup_call_expectations();
    client_.OnStartupComplete();
    run_loop.Run();
  } else {
    // AfterStartupTasks run without startup complete.
    setup_call_expectations();
    post_after_startup_tasks();
    run_loop.Run();
  }
}

TEST_P(AwContentBrowserClientTest,
       OnUiTaskRunnerReadyCallbackRunAfterStartupComplete) {
  StrictMockTask task;

  if (IsAnyExperimentEnabled()) {
    client_.OnUiTaskRunnerReady(task.Get());

    base::RunLoop run_loop;
    EXPECT_CALL(task, Run).WillOnce(
        base::test::RunOnceClosure(run_loop.QuitClosure()));

    client_.OnStartupComplete();

    run_loop.Run();
  } else {
    EXPECT_CALL(task, Run).Times(1);
    client_.OnUiTaskRunnerReady(task.Get());
  }
}

TEST_P(AwContentBrowserClientTest, StartupStatesSetCorrectly) {
  const bool yield_to_native_experiment =
      GetParam() == StartupTaskExperiment::kStartupTasksYieldToNative;

  client_.OnUiTaskRunnerReady(base::DoNothing());
  EXPECT_EQ(yield_to_native_experiment,
            YieldToLooperChecker::GetInstance().ShouldYield());

  client_.OnStartupComplete();
  EXPECT_FALSE(YieldToLooperChecker::GetInstance().ShouldYield());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AwContentBrowserClientTest,
    ::testing::Values(StartupTaskExperiment::kNone,
                      StartupTaskExperiment::kUseStartupTasksLogic,
                      StartupTaskExperiment::kUseStartupTasksLogicP2,
                      StartupTaskExperiment::kStartupTasksYieldToNative),
    StartupTaskExperimentToString);

}  // namespace

}  // namespace android_webview

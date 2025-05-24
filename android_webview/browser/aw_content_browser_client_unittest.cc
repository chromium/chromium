// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include <memory>
#include <vector>

#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/common/aw_switches.h"
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

class AwContentBrowserClientTest : public testing::Test {
 public:
  AwContentBrowserClientTest() {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kWebViewUseStartupTasksLogic);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AwContentBrowserClient client_{
      std::make_unique<AwFeatureListCreator>().get()};
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
};

TEST_F(AwContentBrowserClientTest, ClientTaskNotRunBeforeStartupComplete) {
  StrictMockTask client_task;
  StrictMockTask loop_quitting_task;

  client_.PostAfterStartupTask(FROM_HERE, task_runner_, client_task.Get());

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
}

TEST_F(AwContentBrowserClientTest, TaskRunAfterStartupComplete) {
  StrictMockTask task;

  client_.OnStartupComplete();

  base::RunLoop run_loop;
  EXPECT_CALL(task, Run).WillOnce(
      base::test::RunOnceClosure(run_loop.QuitClosure()));
  client_.PostAfterStartupTask(FROM_HERE, task_runner_, task.Get());

  run_loop.Run();
}

TEST_F(AwContentBrowserClientTest, MultipleTasksBeforeStartup) {
  StrictMockTask task1;
  StrictMockTask task2;
  StrictMockTask task3;

  client_.PostAfterStartupTask(FROM_HERE, task_runner_, task1.Get());
  client_.PostAfterStartupTask(FROM_HERE, task_runner_, task2.Get());
  client_.PostAfterStartupTask(FROM_HERE, task_runner_, task3.Get());

  base::RunLoop run_loop;
  InSequence s;
  EXPECT_CALL(task1, Run);
  EXPECT_CALL(task2, Run);
  EXPECT_CALL(task3, Run)
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  client_.OnStartupComplete();

  run_loop.Run();
}

TEST_F(AwContentBrowserClientTest,
       OnUiTaskRunnerReadyCallbackRunAfterStartupComplete) {
  StrictMockTask task;
  client_.OnUiTaskRunnerReady(task.Get());

  base::RunLoop run_loop;
  EXPECT_CALL(task, Run).WillOnce(
      base::test::RunOnceClosure(run_loop.QuitClosure()));

  client_.OnStartupComplete();

  run_loop.Run();
}

}  // namespace

}  // namespace android_webview

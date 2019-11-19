// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/providers/child_process_task.h"
#include "chrome/browser/task_manager/providers/child_process_task_provider.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using content::ChildProcessData;

namespace task_manager {

namespace {

// Will be used to test the translation from |content::ProcessType| to
// |task_manager::Task::Type|.
struct ProcessTypeTaskTypePair {
  int process_type_;
  Task::Type expected_task_type_;
} process_task_types_pairs[] = {
    { content::PROCESS_TYPE_PPAPI_PLUGIN, Task::PLUGIN },
    { content::PROCESS_TYPE_PPAPI_BROKER, Task::PLUGIN },
    { content::PROCESS_TYPE_UTILITY, Task::UTILITY },
    { content::PROCESS_TYPE_ZYGOTE, Task::ZYGOTE },
    { content::PROCESS_TYPE_SANDBOX_HELPER, Task::SANDBOX_HELPER },
    { content::PROCESS_TYPE_GPU, Task::GPU },
    { PROCESS_TYPE_NACL_LOADER, Task::NACL },
    { PROCESS_TYPE_NACL_BROKER, Task::NACL },
};

}  // namespace

// Defines a test for the child process task provider and the child process
// tasks themselves.
class ChildProcessTaskTest
    : public testing::Test,
      public TaskProviderObserver {
 public:
  ChildProcessTaskTest() {}

  ~ChildProcessTaskTest() override {}

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override {
    DCHECK(task);
    if (provided_tasks_.find(task->process_handle()) != provided_tasks_.end())
      FAIL() << "ChildProcessTaskProvider must never provide duplicate tasks";

    provided_tasks_[task->process_handle()] = task;
  }

  void TaskRemoved(Task* task) override {
    DCHECK(task);
    provided_tasks_.erase(task->process_handle());
  }

  bool AreProviderContainersEmpty(
      const ChildProcessTaskProvider& provider) const {
    return provider.tasks_by_processid_.empty() &&
           provider.tasks_by_child_id_.empty();
  }

 protected:
  std::map<base::ProcessHandle, Task*> provided_tasks_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessTaskTest);
};

// Performs a basic test.
TEST_F(ChildProcessTaskTest, BasicTest) {
  ChildProcessTaskProvider provider;
  EXPECT_TRUE(provided_tasks_.empty());
  provider.SetObserver(this);
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(provided_tasks_.empty()) <<
      "unit tests don't have any browser child processes";
  provider.ClearObserver();
  EXPECT_TRUE(provided_tasks_.empty());
  EXPECT_TRUE(AreProviderContainersEmpty(provider));
}

// Tests everything related to child process task providing.
TEST_F(ChildProcessTaskTest, TestAll) {
  ChildProcessTaskProvider provider;
  EXPECT_TRUE(provided_tasks_.empty());
  provider.SetObserver(this);
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(provided_tasks_.empty());

  // The following process which has handle = base::kNullProcessHandle, won't be
  // added.
  ChildProcessData data1(0);
  ASSERT_FALSE(data1.GetProcess().IsValid());
  provider.BrowserChildProcessLaunchedAndConnected(data1);
  EXPECT_TRUE(provided_tasks_.empty());

  const int unique_id = 245;
  const base::string16 name(base::UTF8ToUTF16("Test Task"));
  const base::string16 expected_name(l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_PLUGIN_PREFIX, name));

  ChildProcessData data2(content::PROCESS_TYPE_PPAPI_PLUGIN);
  data2.SetProcess(base::Process::Current());
  data2.name = name;
  data2.id = unique_id;
  provider.BrowserChildProcessLaunchedAndConnected(data2);
  ASSERT_EQ(1U, provided_tasks_.size());

  Task* task = provided_tasks_.begin()->second;
  // Process handles may not match, but process IDs must:
  EXPECT_EQ(base::GetCurrentProcId(), base::GetProcId(task->process_handle()));
  EXPECT_EQ(base::GetCurrentProcId(), task->process_id());
  EXPECT_EQ(expected_name, task->title());
  EXPECT_EQ(Task::PLUGIN, task->GetType());
  EXPECT_EQ(unique_id, task->GetChildProcessUniqueID());
  EXPECT_EQ(base::string16(), task->GetProfileName());
  EXPECT_FALSE(task->ReportsSqliteMemory());
  EXPECT_FALSE(task->ReportsV8Memory());
  EXPECT_FALSE(task->ReportsWebCacheStats());

  // Make sure that indexing by child_id works properly.
  ASSERT_EQ(task, provider.GetTaskOfUrlRequest(unique_id, 0));
  ASSERT_EQ(task, provider.GetTaskOfUrlRequest(unique_id, 1));

  const int64_t bytes_read = 1024;
  task->OnNetworkBytesRead(bytes_read);
  task->Refresh(base::TimeDelta::FromSeconds(1), REFRESH_TYPE_NETWORK_USAGE);

  EXPECT_EQ(bytes_read, task->network_usage_rate());

  // Clearing the observer won't notify us of any tasks removals even though
  // tasks will be actually deleted.
  provider.ClearObserver();
  EXPECT_FALSE(provided_tasks_.empty());
  EXPECT_TRUE(AreProviderContainersEmpty(provider));
}

// Tests the translation of |content::ProcessType| to
// |task_manager::Task::Type|.
TEST_F(ChildProcessTaskTest, ProcessTypeToTaskType) {
  ChildProcessTaskProvider provider;
  EXPECT_TRUE(provided_tasks_.empty());
  provider.SetObserver(this);
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(provided_tasks_.empty());

  for (const auto& types_pair : process_task_types_pairs) {
    // Add the task.
    ChildProcessData data(types_pair.process_type_);
    data.SetProcess(base::Process::Current());
    provider.BrowserChildProcessLaunchedAndConnected(data);
    ASSERT_EQ(1U, provided_tasks_.size());
    Task* task = provided_tasks_.begin()->second;
    EXPECT_EQ(base::GetCurrentProcId(),
              base::GetProcId(task->process_handle()));
    EXPECT_EQ(types_pair.expected_task_type_, task->GetType());

    // Remove the task.
    provider.BrowserChildProcessHostDisconnected(data);
    EXPECT_TRUE(provided_tasks_.empty());
  }

  provider.ClearObserver();
  EXPECT_TRUE(AreProviderContainersEmpty(provider));
}

}  // namespace task_manager

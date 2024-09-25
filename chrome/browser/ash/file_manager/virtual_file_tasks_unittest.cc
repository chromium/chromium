// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/virtual_tasks/fake_virtual_task.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager::file_tasks {

base::RepeatingCallback<bool()> Return(bool value) {
  return base::BindLambdaForTesting([value] { return value; });
}

class VirtualFileTasksTest : public testing::Test {
 protected:
  VirtualFileTasksTest() {
    task1 = std::make_unique<FakeVirtualTask>(
        ToSwaActionId("id1"),
        /*enabled=*/true, /*matches=*/true,
        /*is_dlp_blocked=*/false, base::BindLambdaForTesting([this]() {
                                    task1_executed_++;
                                  }).Then(Return(true)));
    task2 = std::make_unique<FakeVirtualTask>(
        ToSwaActionId("id2"),
        /*enabled=*/false, /*matches=*/true,
        /*is_dlp_blocked=*/false, base::BindLambdaForTesting([this]() {
                                    task2_executed_++;
                                  }).Then(Return(true))),
    task3 = std::make_unique<FakeVirtualTask>(
        ToSwaActionId("id3"),
        /*enabled=*/true, /*matches=*/true,
        /*is_dlp_blocked=*/false, base::BindLambdaForTesting([this]() {
                                    task3_executed_++;
                                  }).Then(Return(false)));
    task4 = std::make_unique<FakeVirtualTask>(ToSwaActionId("id4"),
                                              /*enabled=*/true,
                                              /*matches=*/false,
                                              /*is_dlp_blocked=*/false,
                                              Return(true));
  }

  void SetUp() override {
    std::vector<VirtualTask*>& tasks = GetTestVirtualTasks();
    tasks.push_back(task1.get());
    tasks.push_back(task2.get());
    tasks.push_back(task3.get());
    tasks.push_back(task4.get());
  }

  void TearDown() override { GetTestVirtualTasks().clear(); }

  std::unique_ptr<FakeVirtualTask> task1;
  std::unique_ptr<FakeVirtualTask> task2;
  std::unique_ptr<FakeVirtualTask> task3;
  std::unique_ptr<FakeVirtualTask> task4;
  int task1_executed_ = 0;
  int task2_executed_ = 0;
  int task3_executed_ = 0;
};

TEST_F(VirtualFileTasksTest, IsVirtualTask_WrongApp) {
  TaskDescriptor wrong_app = {"random_app", TASK_TYPE_WEB_APP, task1->id()};
  ASSERT_FALSE(IsVirtualTask(wrong_app));
}

TEST_F(VirtualFileTasksTest, IsVirtualTask_WrongType) {
  TaskDescriptor wrong_type = {kFileManagerSwaAppId, TASK_TYPE_FILE_HANDLER,
                               task1->id()};
  ASSERT_FALSE(IsVirtualTask(wrong_type));
}

TEST_F(VirtualFileTasksTest, IsVirtualTask_WrongActionId) {
  TaskDescriptor wrong_action_id = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                                    "https://app/wrongaction"};
  ASSERT_FALSE(IsVirtualTask(wrong_action_id));
}

TEST_F(VirtualFileTasksTest, IsVirtualTask_OK) {
  TaskDescriptor ok_task = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                            task1->id()};
  ASSERT_TRUE(IsVirtualTask(ok_task));
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_WrongApp) {
  TaskDescriptor wrong_app = {"random_app", TASK_TYPE_WEB_APP, task1->id()};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, wrong_app, /*file_urls=*/{});
  ASSERT_FALSE(result);
  ASSERT_EQ(task1_executed_, 0);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_WrongActionId) {
  TaskDescriptor wrong_action_id = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                                    "https://app/wrongaction"};
  bool result = ExecuteVirtualTask(/*profile=*/nullptr, wrong_action_id,
                                   /*file_urls=*/{});
  ASSERT_FALSE(result);
  ASSERT_EQ(task1_executed_, 0);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_OK) {
  TaskDescriptor ok_task = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                            task1->id()};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, ok_task, /*file_urls=*/{});
  ASSERT_TRUE(result);
  ASSERT_EQ(task1_executed_, 1);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_NotEnabled) {
  TaskDescriptor disabled_task = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                                  task2->id()};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, disabled_task, /*file_urls=*/{});
  ASSERT_FALSE(result);
  ASSERT_EQ(task2_executed_, 0);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_ExecuteReturnsFalse) {
  TaskDescriptor execute_false = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                                  task3->id()};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, execute_false, /*file_urls=*/{});
  ASSERT_FALSE(result);
  ASSERT_EQ(task3_executed_, 1);
}

TEST_F(VirtualFileTasksTest, MatchVirtualTasks_EmptyFileList) {
  std::vector<FullTaskDescriptor> result_list;
  MatchVirtualTasks(/*profile=*/nullptr, /*entries=*/{},
                    /*file_urls=*/{}, /*dlp_source_urls=*/{}, &result_list);
  ASSERT_EQ(result_list.size(), 0UL);
}

TEST_F(VirtualFileTasksTest, MatchVirtualTasks_OneFile) {
  std::vector<FullTaskDescriptor> result_list;
  MatchVirtualTasks(
      /*profile=*/nullptr, /*entries=*/
      {{base::FilePath("/home/chronos/u-123/MyFiles/foo.txt"), "text/plain",
        /*is_directory=*/false}},
      /*file_urls=*/
      {GURL("filesystem:chrome://file-manager/external/Downloads-123/foo.txt")},
      /*dlp_source_urls=*/{}, &result_list);
  ASSERT_EQ(result_list.size(), 2UL);

  ASSERT_EQ(result_list[0].task_descriptor.action_id, task1->id());
  // Task 2 is disabled.
  ASSERT_EQ(result_list[1].task_descriptor.action_id, task3->id());
  // Task 4 does not match.
}

}  // namespace file_manager::file_tasks

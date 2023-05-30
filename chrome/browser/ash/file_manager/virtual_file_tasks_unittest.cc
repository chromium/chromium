// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager::file_tasks {

class TestVirtualTask : public VirtualTask {
 public:
  TestVirtualTask(base::RepeatingClosure execute,
                  bool execute_result,
                  bool enabled,
                  bool matches,
                  std::string id)
      : execute_(std::move(execute)),
        execute_result_(execute_result),
        enabled_(enabled),
        matches_(matches),
        id_(id) {}

  bool Execute(Profile* profile,
               const TaskDescriptor& task,
               const std::vector<FileSystemURL>& file_urls,
               gfx::NativeWindow modal_parent) const override {
    execute_.Run();
    return execute_result_;
  }

  bool IsEnabled(Profile* profile) const override { return enabled_; }

  bool Matches(const std::vector<extensions::EntryInfo>& entries,
               const std::vector<GURL>& file_urls,
               const std::vector<std::string>& dlp_source_urls) const override {
    return matches_;
  }

  std::string id() const override { return id_; }

  GURL icon_url() const override { return GURL("https://icon_url?"); }

  std::string title() const override { return id() + " title"; }

 private:
  base::RepeatingClosure execute_;
  bool execute_result_;
  bool enabled_;
  bool matches_;
  std::string id_;
};

class VirtualFileTasksTest : public testing::Test {
 protected:
  VirtualFileTasksTest() {
    task1 = std::make_unique<TestVirtualTask>(
        base::BindLambdaForTesting([this]() { task1_executed_++; }),
        /*execute_result=*/true,
        /*enabled=*/true, /*matches=*/true, "https://app/id1");
    task2 = std::make_unique<TestVirtualTask>(
        base::BindLambdaForTesting([this]() { task2_executed_++; }),
        /*execute_result=*/true,
        /*enabled=*/false, /*matches=*/true, "https://app/id2");
    task3 = std::make_unique<TestVirtualTask>(
        base::BindLambdaForTesting([this]() { task3_executed_++; }),
        /*execute_result=*/false,
        /*enabled=*/true, /*matches=*/true, "https://app/id3");
    task4 = std::make_unique<TestVirtualTask>(
        base::DoNothing(), /*execute_result=*/true,
        /*enabled=*/true, /*matches=*/false, "https://app/id4");
  }

  void SetUp() override {
    std::vector<VirtualTask*>& tasks = GetTestVirtualTasks();
    tasks.push_back(task1.get());
    tasks.push_back(task2.get());
    tasks.push_back(task3.get());
    tasks.push_back(task4.get());
  }

  void TearDown() override { GetTestVirtualTasks().clear(); }

  std::unique_ptr<TestVirtualTask> task1;
  std::unique_ptr<TestVirtualTask> task2;
  std::unique_ptr<TestVirtualTask> task3;
  std::unique_ptr<TestVirtualTask> task4;
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
      ExecuteVirtualTask(/*profile=*/nullptr, wrong_app, /*file_urls=*/{},
                         /*modal_parent=*/nullptr);
  ASSERT_FALSE(result);
  ASSERT_EQ(task1_executed_, 0);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_WrongActionId) {
  TaskDescriptor wrong_action_id = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                                    "https://app/wrongaction"};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, wrong_action_id, /*file_urls=*/{},
                         /*modal_parent=*/nullptr);
  ASSERT_FALSE(result);
  ASSERT_EQ(task1_executed_, 0);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_OK) {
  TaskDescriptor ok_task = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                            task1->id()};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, ok_task, /*file_urls=*/{},
                         /*modal_parent=*/nullptr);
  ASSERT_TRUE(result);
  ASSERT_EQ(task1_executed_, 1);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_NotEnabled) {
  TaskDescriptor disabled_task = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                                  task2->id()};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, disabled_task, /*file_urls=*/{},
                         /*modal_parent=*/nullptr);
  ASSERT_FALSE(result);
  ASSERT_EQ(task2_executed_, 0);
}

TEST_F(VirtualFileTasksTest, ExecuteVirtualTask_ExecuteReturnsFalse) {
  TaskDescriptor execute_false = {kFileManagerSwaAppId, TASK_TYPE_WEB_APP,
                                  task3->id()};
  bool result =
      ExecuteVirtualTask(/*profile=*/nullptr, execute_false, /*file_urls=*/{},
                         /*modal_parent=*/nullptr);
  ASSERT_FALSE(result);
  ASSERT_EQ(task3_executed_, 1);
}

TEST_F(VirtualFileTasksTest, FindVirtualTasks_EmptyFileList) {
  std::vector<FullTaskDescriptor> result_list;
  FindVirtualTasks(/*profile=*/nullptr, /*entries=*/{},
                   /*file_urls=*/{}, /*dlp_source_urls=*/{}, &result_list);
  ASSERT_EQ(result_list.size(), 0UL);
}

TEST_F(VirtualFileTasksTest, FindVirtualTasks_OneFile) {
  std::vector<FullTaskDescriptor> result_list;
  FindVirtualTasks(
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

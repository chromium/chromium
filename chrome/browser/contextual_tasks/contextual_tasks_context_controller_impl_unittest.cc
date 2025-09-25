// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_impl.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {
namespace {

using ::testing::_;
using ::testing::Return;

class MockContextualTasksService : public ContextualTasksService {
 public:
  MOCK_METHOD(ContextualTask, CreateTask, (), (override));
  MOCK_METHOD(
      void,
      GetTaskById,
      (const base::Uuid& task_id,
       base::OnceCallback<void(std::optional<ContextualTask>)> callback),
      (const, override));
  MOCK_METHOD(void,
              GetTasks,
              (base::OnceCallback<void(std::vector<ContextualTask>)> callback),
              (const, override));
  MOCK_METHOD(void, DeleteTask, (const base::Uuid& task_id), (override));
  MOCK_METHOD(void,
              AddThreadToTask,
              (const base::Uuid& task_id, const Thread& thread),
              (override));
  MOCK_METHOD(void,
              RemoveThreadFromTask,
              (const base::Uuid& task_id,
               ThreadType type,
               const std::string& server_id),
              (override));
  MOCK_METHOD(void,
              AttachUrlToTask,
              (const base::Uuid& task_id, const GURL& url),
              (override));
  MOCK_METHOD(void,
              DetachUrlFromTask,
              (const base::Uuid& task_id, const GURL& url),
              (override));
  MOCK_METHOD(void,
              AttachSessionIdToTask,
              (const base::Uuid& task_id, SessionID session_id),
              (override));
  MOCK_METHOD(void,
              DetachSessionIdFromTask,
              (const base::Uuid& task_id, SessionID session_id),
              (override));
  MOCK_METHOD(std::optional<ContextualTask>,
              GetMostRecentContextualTaskForSessionID,
              (SessionID session_id),
              (const, override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

class ContextualTasksContextControllerImplTest : public testing::Test {
 public:
  void SetUp() override {
    controller_ =
        std::make_unique<ContextualTasksContextControllerImpl>(&mock_service_);
  }

  void TearDown() override { controller_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockContextualTasksService mock_service_;
  std::unique_ptr<ContextualTasksContextControllerImpl> controller_;

  std::vector<ContextualTask> GetTasks() {
    std::vector<ContextualTask> tasks;
    base::RunLoop run_loop;
    controller_->GetTasks(base::BindOnce(
        [](std::vector<ContextualTask>* out_tasks,
           base::OnceClosure quit_closure, std::vector<ContextualTask> tasks) {
          *out_tasks = std::move(tasks);
          std::move(quit_closure).Run();
        },
        &tasks, run_loop.QuitClosure()));
    run_loop.Run();
    return tasks;
  }

  std::optional<ContextualTask> GetTaskById(const base::Uuid& task_id) {
    std::optional<ContextualTask> task;
    base::RunLoop run_loop;
    controller_->GetTask(task_id,
                         base::BindOnce(
                             [](std::optional<ContextualTask>* out_task,
                                base::OnceClosure quit_closure,
                                std::optional<ContextualTask> result) {
                               *out_task = std::move(result);
                               std::move(quit_closure).Run();
                             },
                             &task, run_loop.QuitClosure()));
    run_loop.Run();
    return task;
  }
};

TEST_F(ContextualTasksContextControllerImplTest, GetTasks) {
  std::vector<ContextualTask> expected_tasks;
  expected_tasks.emplace_back(base::Uuid::GenerateRandomV4());
  expected_tasks.emplace_back(base::Uuid::GenerateRandomV4());

  EXPECT_CALL(mock_service_, GetTasks(_))
      .WillOnce(
          [&](base::OnceCallback<void(std::vector<ContextualTask>)> callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), expected_tasks));
          });

  std::vector<ContextualTask> tasks = GetTasks();
  EXPECT_EQ(tasks.size(), expected_tasks.size());
  for (size_t i = 0; i < tasks.size(); ++i) {
    EXPECT_EQ(tasks[i].GetTaskId(), expected_tasks[i].GetTaskId());
  }
}

TEST_F(ContextualTasksContextControllerImplTest, GetTask) {
  ContextualTask expected_task(base::Uuid::GenerateRandomV4());
  base::Uuid task_id = expected_task.GetTaskId();

  EXPECT_CALL(mock_service_, GetTaskById(task_id, _))
      .WillOnce([&](const base::Uuid&,
                    base::OnceCallback<void(std::optional<ContextualTask>)>
                        callback) {
        std::move(callback).Run(std::make_optional(expected_task));
      });

  std::optional<ContextualTask> task = GetTaskById(task_id);
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->GetTaskId(), expected_task.GetTaskId());
}

TEST_F(ContextualTasksContextControllerImplTest, GetTask_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(mock_service_, GetTaskById(task_id, _))
      .WillOnce([&](const base::Uuid&,
                    base::OnceCallback<void(std::optional<ContextualTask>)>
                        callback) { std::move(callback).Run(std::nullopt); });

  std::optional<ContextualTask> task = GetTaskById(task_id);
  EXPECT_FALSE(task.has_value());
}

}  // namespace
}  // namespace contextual_tasks

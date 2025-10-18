// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {
namespace {

const char kTestUrl[] = "https://google.com";

using ::testing::_;
using ::testing::Return;

class MockContextualTasksService : public ContextualTasksService {
 public:
  MOCK_METHOD(ContextualTask, CreateTask, (), (override));
  MOCK_METHOD(ContextualTask, CreateTaskFromUrl, (const GURL& url), (override));
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
              UpdateThreadTurnId,
              (const base::Uuid& task_id,
               ThreadType thread_type,
               const std::string& server_id,
               const std::string& conversation_turn_id),
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
              GetContextForTask,
              (const base::Uuid& task_id,
               const std::set<ContextualTaskContextSource>& sources,
               base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                   context_callback),
              (override));
  MOCK_METHOD(void,
              AssociateTabWithTask,
              (const base::Uuid& task_id, SessionID tab_id),
              (override));
  MOCK_METHOD(void,
              DisassociateTabFromTask,
              (const base::Uuid& task_id, SessionID tab_id),
              (override));
  MOCK_METHOD(std::optional<ContextualTask>,
              GetContextualTaskForTab,
              (SessionID tab_id),
              (const, override));
  MOCK_METHOD(void,
              ClearAllTabAssociationsForTask,
              (const base::Uuid& task_id),
              (override));

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetAiThreadControllerDelegate,
              (),
              (override));
  MOCK_METHOD(FeatureEligibility, GetFeatureEligibility, (), (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
};

class ContextualTasksContextControllerImplTest : public testing::Test {
 public:
  void SetUp() override {
    controller_ =
        std::make_unique<ContextualTasksContextControllerImpl>(&mock_service_);
  }

  void TearDown() override { controller_.reset(); }

 protected:
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
    controller_->GetTaskById(task_id,
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

  std::unique_ptr<ContextualTaskContext> GetContextForTask(
      const base::Uuid& task_id) {
    std::unique_ptr<ContextualTaskContext> result;
    base::RunLoop run_loop;
    controller_->GetContextForTask(
        task_id, {},
        base::BindOnce(
            [](std::unique_ptr<ContextualTaskContext>* out_context,
               base::OnceClosure quit_closure,
               std::unique_ptr<ContextualTaskContext> context) {
              *out_context = std::move(context);
              std::move(quit_closure).Run();
            },
            &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  // Mock service to control the behavior of ContextualTasksService.
  MockContextualTasksService mock_service_;
  // The controller under test.
  std::unique_ptr<ContextualTasksContextControllerImpl> controller_;
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

TEST_F(ContextualTasksContextControllerImplTest, GetTaskById) {
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

TEST_F(ContextualTasksContextControllerImplTest, GetTaskById_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(mock_service_, GetTaskById(task_id, _))
      .WillOnce([&](const base::Uuid&,
                    base::OnceCallback<void(std::optional<ContextualTask>)>
                        callback) { std::move(callback).Run(std::nullopt); });

  std::optional<ContextualTask> task = GetTaskById(task_id);
  EXPECT_FALSE(task.has_value());
}

TEST_F(ContextualTasksContextControllerImplTest, CreateTaskFromUrl) {
  ContextualTask expected_task(base::Uuid::GenerateRandomV4());
  EXPECT_CALL(mock_service_, CreateTaskFromUrl(GURL(kTestUrl)))
      .WillOnce(Return(expected_task));
  ContextualTask task = controller_->CreateTaskFromUrl(GURL(kTestUrl));
  EXPECT_EQ(task.GetTaskId(), expected_task.GetTaskId());
}

TEST_F(ContextualTasksContextControllerImplTest, AddThreadToTask) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ThreadType thread_type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string conversation_turn_id = "conversation_turn_id";
  std::string title = "title";
  Thread thread(thread_type, server_id, title, conversation_turn_id);

  EXPECT_CALL(mock_service_, AddThreadToTask(task_id, _))
      .WillOnce([&](const base::Uuid&, const Thread& passed_thread) {
        EXPECT_EQ(passed_thread.type, thread_type);
        EXPECT_EQ(passed_thread.server_id, server_id);
        EXPECT_EQ(passed_thread.title, title);
        EXPECT_EQ(passed_thread.conversation_turn_id, conversation_turn_id);
      });
  controller_->AddThreadToTask(task_id, thread);
}

TEST_F(ContextualTasksContextControllerImplTest, UpdateThreadTurnId) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ThreadType thread_type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string conversation_turn_id = "conversation_turn_id";

  EXPECT_CALL(mock_service_, UpdateThreadTurnId(task_id, thread_type, server_id,
                                                conversation_turn_id))
      .Times(1);
  controller_->UpdateThreadTurnId(task_id, thread_type, server_id,
                                  conversation_turn_id);
}

TEST_F(ContextualTasksContextControllerImplTest, AssociateTabWithTask) {
  SessionID tab_session_id = SessionID::NewUnique();
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(mock_service_, AssociateTabWithTask(task_id, tab_session_id))
      .Times(1);

  controller_->AssociateTabWithTask(task_id, tab_session_id);
}

TEST_F(ContextualTasksContextControllerImplTest, GetContextualTaskForTab) {
  SessionID tab_session_id = SessionID::NewUnique();
  ContextualTask expected_task(base::Uuid::GenerateRandomV4());

  EXPECT_CALL(mock_service_, GetContextualTaskForTab(tab_session_id))
      .WillOnce(Return(std::make_optional(expected_task)));

  std::optional<ContextualTask> task =
      controller_->GetContextualTaskForTab(tab_session_id);
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->GetTaskId(), expected_task.GetTaskId());
}

TEST_F(ContextualTasksContextControllerImplTest,
       GetContextualTaskForTab_NotFound) {
  SessionID tab_session_id = SessionID::NewUnique();

  EXPECT_CALL(mock_service_, GetContextualTaskForTab(tab_session_id))
      .WillOnce(Return(std::nullopt));

  std::optional<ContextualTask> task =
      controller_->GetContextualTaskForTab(tab_session_id);
  EXPECT_FALSE(task.has_value());
}

TEST_F(ContextualTasksContextControllerImplTest, AttachUrlToTask) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL url("https://google.com");

  EXPECT_CALL(mock_service_, AttachUrlToTask(task_id, url)).Times(1);

  controller_->AttachUrlToTask(task_id, url);
}

TEST_F(ContextualTasksContextControllerImplTest, DetachUrlFromTask) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL url("https://google.com");

  EXPECT_CALL(mock_service_, DetachUrlFromTask(task_id, url)).Times(1);

  controller_->DetachUrlFromTask(task_id, url);
}

TEST_F(ContextualTasksContextControllerImplTest, GetContextForTask) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);

  GURL url1("https://example.com/1");
  GURL url2("https://example.com/2");
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url1));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url2));

  ContextualTaskContext expected_context(task);

  EXPECT_CALL(mock_service_, GetContextForTask(task_id, _, _))
      .WillOnce(
          [&](const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) {
            std::move(callback).Run(
                std::make_unique<ContextualTaskContext>(expected_context));
          });

  std::unique_ptr<ContextualTaskContext> context = GetContextForTask(task_id);
  ASSERT_TRUE(context.get());
  EXPECT_EQ(context->GetTaskId(), expected_context.GetTaskId());
  const auto& attachments = context->GetUrlAttachments();
  ASSERT_EQ(attachments.size(), 2u);
  EXPECT_EQ(attachments[0].GetURL(), url1);
  EXPECT_EQ(attachments[1].GetURL(), url2);
}

TEST_F(ContextualTasksContextControllerImplTest, GetContextForTask_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(mock_service_, GetContextForTask(task_id, _, _))
      .WillOnce(
          [&](const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(nullptr); });

  std::unique_ptr<ContextualTaskContext> context = GetContextForTask(task_id);
  EXPECT_FALSE(context);
}

TEST_F(ContextualTasksContextControllerImplTest, GetFeatureEligibility) {
  const FeatureEligibility expected_eligibility = {true, false};
  EXPECT_CALL(mock_service_, GetFeatureEligibility())
      .WillOnce(Return(expected_eligibility));

  const FeatureEligibility actual_eligibility =
      controller_->GetFeatureEligibility();
  EXPECT_EQ(actual_eligibility.contextual_tasks_enabled,
            expected_eligibility.contextual_tasks_enabled);
  EXPECT_EQ(actual_eligibility.aim_eligible, expected_eligibility.aim_eligible);
}

}  // namespace
}  // namespace contextual_tasks

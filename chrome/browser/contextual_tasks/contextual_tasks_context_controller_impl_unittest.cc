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
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_context_controller.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
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
        task_id, {}, nullptr,
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

  std::unique_ptr<ContextualTaskContext> GetContextForTaskWithParams(
      const base::Uuid& task_id,
      std::unique_ptr<ContextDecorationParams> params) {
    std::unique_ptr<ContextualTaskContext> result;
    base::RunLoop run_loop;
    controller_->GetContextForTask(
        task_id, {}, std::move(params),
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
  MockContextualTasksContextController mock_service_;
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

TEST_F(ContextualTasksContextControllerImplTest,
       UpdateThreadForTask_AddsThread) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ThreadType thread_type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string conversation_turn_id = "conversation_turn_id";
  std::string title = "title";

  EXPECT_CALL(mock_service_,
              UpdateThreadForTask(task_id, thread_type, server_id,
                                  std::make_optional(conversation_turn_id),
                                  std::make_optional(title)))
      .Times(1);
  controller_->UpdateThreadForTask(task_id, thread_type, server_id,
                                   conversation_turn_id, title);
}

TEST_F(ContextualTasksContextControllerImplTest,
       UpdateThreadForTask_UpdatesThread) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ThreadType thread_type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string old_conversation_turn_id = "old_conversation_turn_id";
  std::string new_conversation_turn_id = "new_conversation_turn_id";
  std::string old_title = "old_title";
  std::string new_title = "new_title";

  EXPECT_CALL(mock_service_,
              UpdateThreadForTask(task_id, thread_type, server_id,
                                  std::make_optional(new_conversation_turn_id),
                                  std::make_optional(new_title)))
      .Times(1);
  controller_->UpdateThreadForTask(task_id, thread_type, server_id,
                                   new_conversation_turn_id, new_title);
}

TEST_F(ContextualTasksContextControllerImplTest, GetTaskFromServerId) {
  ThreadType thread_type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  ContextualTask expected_task(base::Uuid::GenerateRandomV4());

  EXPECT_CALL(mock_service_, GetTaskFromServerId(thread_type, server_id))
      .WillOnce(Return(std::make_optional(expected_task)));

  std::optional<ContextualTask> task =
      controller_->GetTaskFromServerId(thread_type, server_id);
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->GetTaskId(), expected_task.GetTaskId());
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

  EXPECT_CALL(mock_service_, GetContextForTask(task_id, _, _, _))
      .WillOnce(
          [&](const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams> params,
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

  EXPECT_CALL(mock_service_, GetContextForTask(task_id, _, _, _))
      .WillOnce(
          [&](const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams> params,
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

TEST_F(ContextualTasksContextControllerImplTest, GetContextForTask_WithParams) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);

  ContextualTaskContext expected_context(task);
  auto input_params = std::make_unique<ContextDecorationParams>();
  // Extract raw pointer to be able to verify what it points to within the
  // lambda for the service API call.
  ContextDecorationParams* input_params_ptr = input_params.get();

  EXPECT_CALL(mock_service_, GetContextForTask(task_id, _, _, _))
      .WillOnce(
          [&](const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) {
            // Verify that params is not null and are actually the ones that
            // provided.
            ASSERT_TRUE(params);
            EXPECT_EQ(input_params_ptr, params.get());
            std::move(callback).Run(
                std::make_unique<ContextualTaskContext>(expected_context));
          });

  // Call the GetContextForTask method with the input_params.
  std::unique_ptr<ContextualTaskContext> context =
      GetContextForTaskWithParams(task_id, std::move(input_params));
  ASSERT_TRUE(context.get());
  EXPECT_EQ(context->GetTaskId(), expected_context.GetTaskId());
}

}  // namespace
}  // namespace contextual_tasks

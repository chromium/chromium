// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_impl.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {
namespace {

using ::testing::_;
using ::testing::Return;

class MockAimEligibilityService : public AimEligibilityService {
 public:
  explicit MockAimEligibilityService(PrefService* pref_service)
      : AimEligibilityService(*pref_service, nullptr, nullptr, nullptr) {}
  MOCK_METHOD(bool, IsAimEligible, (), (const, override));

  // The following methods are marked as pure virtual in AimEligibilityService,
  // as they are implemented in ChromeAimEligibilityService which is the one
  // provided by the KeyedService factory. We therefore need to implement them
  // in this unit test.
  std::string GetCountryCode() const override { return "US"; }
  std::string GetLocale() const override { return "en-US"; }
};

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
              GetContextForTask,
              (const base::Uuid& task_id,
               const std::set<ContextualTaskContextSource>& sources,
               base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                   context_callback),
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
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetAiThreadControllerDelegate,
              (),
              (override));
};

class ContextualTasksContextControllerImplTest : public testing::Test {
 public:
  void SetUp() override {
    AimEligibilityService::RegisterProfilePrefs(pref_service_.registry());
    mock_aim_eligibility_service_ =
        std::make_unique<MockAimEligibilityService>(&pref_service_);
    controller_ = std::make_unique<ContextualTasksContextControllerImpl>(
        &mock_service_, mock_aim_eligibility_service_.get());
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

  std::optional<ContextualTask> GetSelectedTaskForTab(
      SessionID tab_session_id) {
    std::optional<ContextualTask> task;
    base::RunLoop run_loop;
    controller_->GetSelectedTaskForTab(
        tab_session_id, base::BindOnce(
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

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  // Mock service to control the behavior of ContextualTasksService.
  MockContextualTasksService mock_service_;
  // Mock service to control the behavior of AimEligibilityService.
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service_;
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

TEST_F(ContextualTasksContextControllerImplTest, AssociateTabWithTask) {
  SessionID tab_session_id = SessionID::NewUnique();
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(mock_service_, AttachSessionIdToTask(task_id, tab_session_id))
      .Times(1);

  controller_->AssociateTabWithTask(tab_session_id, task_id);
}

TEST_F(ContextualTasksContextControllerImplTest, GetSelectedTaskForTab) {
  SessionID tab_session_id = SessionID::NewUnique();
  ContextualTask expected_task(base::Uuid::GenerateRandomV4());

  EXPECT_CALL(mock_service_,
              GetMostRecentContextualTaskForSessionID(tab_session_id))
      .WillOnce(Return(std::make_optional(expected_task)));

  std::optional<ContextualTask> task = GetSelectedTaskForTab(tab_session_id);
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->GetTaskId(), expected_task.GetTaskId());
}

TEST_F(ContextualTasksContextControllerImplTest,
       GetSelectedTaskForTab_NotFound) {
  SessionID tab_session_id = SessionID::NewUnique();

  EXPECT_CALL(mock_service_,
              GetMostRecentContextualTaskForSessionID(tab_session_id))
      .WillOnce(Return(std::nullopt));

  std::optional<ContextualTask> task = GetSelectedTaskForTab(tab_session_id);
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
  // Test case 1: Feature flag enabled, AIM eligible.
  feature_list_.InitAndEnableFeature(kContextualTasks);
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(true));
  EXPECT_TRUE(controller_->GetFeatureEligibility().IsEligible());

  // Test case 2: Feature flag enabled, AIM not eligible.
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(false));
  EXPECT_FALSE(controller_->GetFeatureEligibility().IsEligible());

  feature_list_.Reset();
  // Test case 3: Feature flag disabled, AIM eligible.
  feature_list_.InitAndDisableFeature(kContextualTasks);
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(true));
  EXPECT_FALSE(controller_->GetFeatureEligibility().IsEligible());

  // Test case 4: Feature flag disabled, AIM not eligible.
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(false));
  EXPECT_FALSE(controller_->GetFeatureEligibility().IsEligible());
}

}  // namespace
}  // namespace contextual_tasks

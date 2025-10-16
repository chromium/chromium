// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <memory>
#include <optional>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_state_manager.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

using ::testing::_;

std::unique_ptr<ui::ActorUiStateManagerInterface> BuildUiStateManagerMock() {
  std::unique_ptr<ui::MockActorUiStateManager> ui_state_manager =
      std::make_unique<ui::MockActorUiStateManager>();
  ON_CALL(*ui_state_manager, OnUiEvent(_, _))
      .WillByDefault([](ui::AsyncUiEvent, ui::UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      });
  return ui_state_manager;
}

constexpr char kActorTaskCreatedHistogram[] = "Actor.Task.Created";

class ActorKeyedServiceTest : public testing::Test {
 public:
  ActorKeyedServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ActorKeyedServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager()->CreateTestingProfile("profile");
    ActorKeyedService::Get(profile())->SetActorUiStateManagerForTesting(
        BuildUiStateManagerMock());
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  base::CallbackListSubscription user_confirmation_dialog_subscription_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

// Adds a task to ActorKeyedService
TEST_F(ActorKeyedServiceTest, AddActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  actor_service->CreateTask();
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 1u);
  EXPECT_EQ(actor_service->GetActiveTasks().begin()->second->GetState(),
            ActorTask::State::kCreated);
}

// Stops a task.
TEST_F(ActorKeyedServiceTest, StopActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTask();

  // Add a tab to the task
  ActorTask* task = actor_service->GetTask(id);
  base::RunLoop loop;
  task->AddTab(tabs::TabHandle(123),
               base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
                 EXPECT_TRUE(IsOk(*result));
                 loop.Quit();
               }));
  loop.Run();

  EXPECT_TRUE(task->IsActingOnTab(tabs::TabHandle(123)));
  EXPECT_TRUE(task->HasTab(tabs::TabHandle(123)));
  actor_service->StopTask(id, /*success=*/true);
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 0u);
  ASSERT_EQ(actor_service->GetInactiveTasks().size(), 1u);
  EXPECT_EQ(actor_service->GetInactiveTasks().begin()->second->GetState(),
            ActorTask::State::kFinished);
  EXPECT_EQ(actor_service->GetInactiveTasks().begin()->second->GetEndTime(),
            base::Time::Now());
  EXPECT_FALSE(task->IsActingOnTab(tabs::TabHandle(123)));
  EXPECT_FALSE(task->HasTab(tabs::TabHandle(123)));
}

TEST_F(ActorKeyedServiceTest, FindTaskIdsInActive_ReturnsSuccessfully) {
  auto* actor_service = ActorKeyedService::Get(profile());
  actor_service->CreateTask();
  const TaskId id2 = actor_service->CreateTask();
  actor_service->GetTask(id2)->Pause(/*from_actor=*/true);

  // Find a single active task.
  std::vector<TaskId> single_found =
      actor_service->FindTaskIdsInActive([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kPausedByActor;
      });
  ASSERT_EQ(single_found.size(), 1u);
  EXPECT_EQ(single_found[0], id2);
}

TEST_F(ActorKeyedServiceTest, FindTaskIdsInInactive_ReturnsSuccessfully) {
  auto* actor_service = ActorKeyedService::Get(profile());
  const TaskId id1 = actor_service->CreateTask();
  const TaskId id2 = actor_service->CreateTask();
  actor_service->StopTask(id1, /*success=*/true);
  actor_service->StopTask(id2, /*success=*/false);

  // Find a single inactive task.
  std::vector<TaskId> single_found =
      actor_service->FindTaskIdsInInactive([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kCancelled;
      });
  ASSERT_EQ(single_found.size(), 1u);
  EXPECT_EQ(single_found[0], id2);
}

// Test that adding a tab to a paused or stopped task has no effect.
TEST_F(ActorKeyedServiceTest, AddTabToPausedOrStoppedTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTask();

  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  const tabs::TabHandle tab_handle(123);

  // Pause the task and try to add a tab.
  task->Pause(/*from_actor=*/true);
  EXPECT_TRUE(task->IsPaused());

  {
    base::RunLoop loop;
    task->AddTab(tab_handle,
                 base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
                   EXPECT_EQ(result->code,
                             mojom::ActionResultCode::kTaskPaused);
                   loop.Quit();
                 }));
    loop.Run();
  }
  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_FALSE(task->HasTab(tab_handle));

  // Stop the task and try to add a tab.
  actor_service->StopTask(id, true);
  EXPECT_TRUE(task->IsStopped());
  {
    base::RunLoop loop;
    task->AddTab(tab_handle,
                 base::BindLambdaForTesting([&](mojom::ActionResultPtr result) {
                   EXPECT_EQ(result->code,
                             mojom::ActionResultCode::kTaskWentAway);
                   loop.Quit();
                 }));
    loop.Run();
  }
  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_FALSE(task->HasTab(tab_handle));
}

// Test tab association to a paused task.
TEST_F(ActorKeyedServiceTest, PausedTaskTabs) {
  auto* actor_service = ActorKeyedService::Get(profile());
  TaskId id = actor_service->CreateTask();

  ActorTask* task = actor_service->GetTask(id);
  ASSERT_TRUE(task);
  const tabs::TabHandle tab_handle(123);

  {
    base::test::TestFuture<mojom::ActionResultPtr> future;
    task->AddTab(tab_handle, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // The tab should be both part of the task and actively acting on it when in a
  // created or acting state.

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->SetState(ActorTask::State::kActing);

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->SetState(ActorTask::State::kReflecting);

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  // Pausing the task should keep the tab in the task but it should no longer be
  // considered acting.

  task->Pause(true);

  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->Resume();

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->Pause(false);

  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  task->Resume();

  EXPECT_TRUE(task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(task->HasTab(tab_handle));

  // Stop the task. This should remove the tab from the task.
  actor_service->StopTask(id, true);

  EXPECT_FALSE(task->IsActingOnTab(tab_handle));
  EXPECT_FALSE(task->HasTab(tab_handle));
}

TEST_F(ActorKeyedServiceTest, LogsActorTaskCreatedOnCreateTask) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kActorTaskCreatedHistogram, 0);

  ActorKeyedService::Get(profile())->CreateTask();

  histogram_tester.ExpectBucketCount(kActorTaskCreatedHistogram, true, 1);
}

}  // namespace

}  // namespace actor

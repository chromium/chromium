// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <memory>
#include <optional>

#include "base/test/bind.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/mock_actor_ui_state_manager.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
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
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

// Adds a task to ActorKeyedService
TEST_F(ActorKeyedServiceTest, AddActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  actor_service->SetActorUiStateManagerForTesting(BuildUiStateManagerMock());
  std::unique_ptr<ExecutionEngine> execution_engine =
      std::make_unique<ExecutionEngine>(profile());
  actor_service->AddActiveTask(std::make_unique<ActorTask>(
      profile(), std::move(execution_engine),
      ui::NewUiEventDispatcher(actor_service->GetActorUiStateManager())));
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 1u);
  EXPECT_EQ(actor_service->GetActiveTasks().begin()->second->GetState(),
            ActorTask::State::kCreated);
}

// Stops a task.
TEST_F(ActorKeyedServiceTest, StopActiveTask) {
  auto* actor_service = ActorKeyedService::Get(profile());
  actor_service->SetActorUiStateManagerForTesting(BuildUiStateManagerMock());
  std::unique_ptr<ExecutionEngine> execution_engine =
      std::make_unique<ExecutionEngine>(profile());
  TaskId id = actor_service->AddActiveTask(std::make_unique<ActorTask>(
      profile(), std::move(execution_engine),
      ui::NewUiEventDispatcher(actor_service->GetActorUiStateManager())));

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
  actor_service->StopTask(id);
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 0u);
  ASSERT_EQ(actor_service->GetInactiveTasks().size(), 1u);
  EXPECT_EQ(actor_service->GetInactiveTasks().begin()->second->GetState(),
            ActorTask::State::kFinished);
  EXPECT_EQ(actor_service->GetInactiveTasks().begin()->second->GetEndTime(),
            base::Time::Now());
  EXPECT_FALSE(task->IsActingOnTab(tabs::TabHandle(123)));
}

}  // namespace

}  // namespace actor

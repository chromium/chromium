// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/test/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/ui/mock_actor_ui_state_manager.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {
using ::tabs::MockTabInterface;
using ::testing::_;
using ::testing::Return;

class ActorUiTabControllerTest : public testing::Test {
 public:
  ActorUiTabControllerTest() = default;
  ~ActorUiTabControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    // TODO(crbug.com/425952887): Refactor unit test to get rid of
    // TestingFactory and pass in the actor_keyed_service() directly.
    profile_ = TestingProfile::Builder()
                   .AddTestingFactory(
                       ActorKeyedServiceFactory::GetInstance(),
                       base::BindRepeating(
                           &ActorUiTabControllerTest::BuildActorKeyedService,
                           base::Unretained(this)))
                   .Build();
    actor_ui_tab_controller_ = std::make_unique<ActorUiTabController>(
        mock_tab_, actor_keyed_service());
    ON_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(mock_browser_window_interface_, GetProfile)
        .WillByDefault(Return(profile()));
    task_id_ = actor_keyed_service()->CreateTaskForTesting();
    actor_ui_tab_controller_->SetActiveTaskId(task_id_);
    base::RunLoop loop;
    actor_keyed_service()->GetTask(task_id_)->AddTab(
        mock_tab_.GetHandle(),
        base::BindLambdaForTesting([&](::actor::mojom::ActionResultPtr result) {
          EXPECT_TRUE(IsOk(*result));
          loop.Quit();
        }));
    loop.Run();
  }

  std::unique_ptr<KeyedService> BuildActorKeyedService(
      content::BrowserContext* context) {
    auto actor_keyed_service =
        std::make_unique<ActorKeyedServiceFake>(static_cast<Profile*>(context));
    std::unique_ptr<MockActorUiStateManager> ausm =
        std::make_unique<MockActorUiStateManager>();
    ON_CALL(*ausm, GetUiTabController(_))
        .WillByDefault(Return(actor_ui_tab_controller()));
    actor_keyed_service->SetActorUiStateManagerForTesting(std::move(ausm));
    return std::move(actor_keyed_service);
  }

  ActorKeyedServiceFake* actor_keyed_service() {
    return static_cast<ActorKeyedServiceFake*>(
        ActorKeyedService::Get(profile()));
  }

  ActorUiTabControllerInterface* actor_ui_tab_controller() {
    return actor_ui_tab_controller_.get();
  }

  ActorUiStateManagerInterface* actor_ui_state_manager() {
    return ActorKeyedService::Get(profile())->GetActorUiStateManager();
  }

  TaskId task_id() { return task_id_; }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorUiTabControllerInterface> actor_ui_tab_controller_;
  MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  TaskId task_id_;
};

TEST_F(ActorUiTabControllerTest, SetActorTaskStatePaused_SetsStateCorrectly) {
  actor_ui_tab_controller()->SetActorTaskPaused();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kPausedByClient);
}

TEST_F(ActorUiTabControllerTest, SetActorTaskStateResume_SetsStateCorrectly) {
  actor_ui_tab_controller()->SetActorTaskResume();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kReflecting);
}

}  // namespace
}  // namespace actor::ui

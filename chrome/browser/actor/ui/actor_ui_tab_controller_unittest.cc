// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/test/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/mock_actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/mock_handoff_button_controller.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {
using ::tabs::MockTabInterface;
using ::testing::_;
using ::testing::Return;

class ActorUiTabControllerFake : public ActorUiTabController {
 public:
  explicit ActorUiTabControllerFake(tabs::TabInterface& tab,
                                    ActorKeyedService* actor_service)
      : ActorUiTabController(tab, actor_service) {
    handoff_button_controller_ =
        std::make_unique<MockHandoffButtonController>(tab);

    mock_handoff_button_controller_ = static_cast<MockHandoffButtonController*>(
        handoff_button_controller_.get());
  }

  MockHandoffButtonController* mock_handoff_button_controller() {
    return mock_handoff_button_controller_;
  }

 private:
  raw_ptr<MockHandoffButtonController> mock_handoff_button_controller_;
};

class ActorUiTabControllerTest : public testing::Test {
 public:
  ActorUiTabControllerTest() = default;
  ~ActorUiTabControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiHandoffButtonName, "true"}});
    // TODO(crbug.com/425952887): Refactor unit test to get rid of
    // TestingFactory and pass in the actor_keyed_service() directly.
    profile_ = TestingProfile::Builder()
                   .AddTestingFactory(
                       ActorKeyedServiceFactory::GetInstance(),
                       base::BindRepeating(
                           &ActorUiTabControllerTest::BuildActorKeyedService,
                           base::Unretained(this)))
                   .Build();
    actor_ui_tab_controller_ = std::make_unique<ActorUiTabControllerFake>(
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

  ActorUiTabControllerFake* actor_ui_tab_controller() {
    return actor_ui_tab_controller_.get();
  }

  ActorUiStateManagerInterface* actor_ui_state_manager() {
    return ActorKeyedService::Get(profile())->GetActorUiStateManager();
  }

  MockHandoffButtonController* mock_handoff_button_controller() {
    return static_cast<MockHandoffButtonController*>(
        actor_ui_tab_controller()->mock_handoff_button_controller());
  }

  TaskId task_id() { return task_id_; }

  TestingProfile* profile() { return profile_.get(); }

  MockTabInterface& mock_tab() { return mock_tab_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorUiTabControllerFake> actor_ui_tab_controller_;
  MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_F(ActorUiTabControllerTest, OnTabActiveStatusChanged_ChangesVisibility) {
  // The tab is inactive, so setting it active should make it visible.
  EXPECT_CALL(*mock_handoff_button_controller(),
              UpdateState(HandoffButtonState(), true));
  actor_ui_tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());

  // Now the tab is active. Setting it to inactive should make it invisible.
  EXPECT_CALL(*mock_handoff_button_controller(),
              UpdateState(HandoffButtonState(), false));
  actor_ui_tab_controller()->OnTabActiveStatusChanged(false, &mock_tab());
}

TEST_F(ActorUiTabControllerTest,
       OnUiTabStateChange_CallsHandoffControllerWithCorrectStateAndVisibility) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kAgent);
  UiTabState ui_tab_state = UiTabState(
      ActorOverlayState(true, false, std::nullopt), handoff_button_state);
  // First, set the tab's status to inactive.
  ON_CALL(mock_tab(), IsActivated).WillByDefault(Return(false));

  // The visibility will be false since the tab is inactive.
  EXPECT_CALL(*mock_handoff_button_controller(),
              UpdateState(handoff_button_state, false));

  actor_ui_tab_controller()->OnUiTabStateChange(ui_tab_state,
                                                base::DoNothing());
}

TEST_F(ActorUiTabControllerTest, OnUiTabStateChange_NoOpIfStateIsUnchanged) {
  UiTabState ui_tab_state = UiTabState(
      ActorOverlayState(true, false, std::nullopt),
      HandoffButtonState(true, HandoffButtonState::ControlOwnership::kAgent));

  EXPECT_CALL(*mock_handoff_button_controller(), UpdateState(_, _)).Times(1);

  actor_ui_tab_controller()->OnUiTabStateChange(ui_tab_state,
                                                base::DoNothing());

  actor_ui_tab_controller()->OnUiTabStateChange(ui_tab_state,
                                                base::DoNothing());
}

TEST_F(ActorUiTabControllerTest,
       SetHandoffButtonVisibility_TrueWhenTabIsActiveAndInputIsTrue) {
  // First, ensure the tab is active.
  actor_ui_tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());

  // Expect UpdateState to be called with is_visible set to true.
  EXPECT_CALL(*mock_handoff_button_controller(),
              UpdateState(HandoffButtonState(), true));

  actor_ui_tab_controller()->SetHandoffButtonVisibility(true);
}

TEST_F(ActorUiTabControllerTest,
       SetHandoffButtonVisibility_FalseWhenTabIsActiveAndInputIsFalse) {
  // First, ensure the tab is active.
  actor_ui_tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());

  // Expect UpdateState to be called with is_visible set to false.
  EXPECT_CALL(*mock_handoff_button_controller(),
              UpdateState(HandoffButtonState(), false));

  actor_ui_tab_controller()->SetHandoffButtonVisibility(false);
}

TEST_F(ActorUiTabControllerTest,
       SetHandoffButtonVisibility_AlwaysFalseWhenTabIsInactive) {
  // First, ensure the tab is inactive.
  actor_ui_tab_controller()->OnTabActiveStatusChanged(false, &mock_tab());

  // Expect UpdateState to be called with is_visible set to false.
  EXPECT_CALL(*mock_handoff_button_controller(),
              UpdateState(HandoffButtonState(), false))
      .Times(2);

  actor_ui_tab_controller()->SetHandoffButtonVisibility(true);
  actor_ui_tab_controller()->SetHandoffButtonVisibility(false);
}

}  // namespace
}  // namespace actor::ui

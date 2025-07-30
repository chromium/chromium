// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/mock_actor_overlay_view_controller.h"
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

class ActorUiTabControllerTest : public testing::Test {
 public:
  ActorUiTabControllerTest() = default;
  ~ActorUiTabControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiHandoffButtonName, "true"},
         {features::kGlicActorUiOverlayName, "true"}});
    // TODO(crbug.com/425952887): Refactor unit test to get rid of
    // TestingFactory and pass in the actor_keyed_service() directly.
    profile_ = TestingProfile::Builder()
                   .AddTestingFactory(
                       ActorKeyedServiceFactory::GetInstance(),
                       base::BindRepeating(
                           &ActorUiTabControllerTest::BuildActorKeyedService,
                           base::Unretained(this)))
                   .Build();
    auto handoff_button_controller =
        std::make_unique<MockHandoffButtonController>(mock_tab());
    mock_handoff_button_controller_ = handoff_button_controller.get();
    auto actor_overlay_view_controller =
        std::make_unique<MockActorOverlayViewController>(mock_tab());
    mock_actor_overlay_view_controller_ = actor_overlay_view_controller.get();

    actor_ui_tab_controller_ = std::make_unique<ActorUiTabController>(
        mock_tab_, actor_keyed_service(),
        std::move(actor_overlay_view_controller),
        std::move(handoff_button_controller));
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
        .WillByDefault(Return(actor_ui_tab_controller_.get()));
    actor_keyed_service->SetActorUiStateManagerForTesting(std::move(ausm));
    return std::move(actor_keyed_service);
  }

  ActorKeyedServiceFake* actor_keyed_service() {
    return static_cast<ActorKeyedServiceFake*>(
        ActorKeyedService::Get(profile()));
  }

  TaskId task_id() { return task_id_; }

  TestingProfile* profile() { return profile_.get(); }

  MockTabInterface& mock_tab() { return mock_tab_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TaskId task_id_;

 protected:
  std::unique_ptr<ActorUiTabController> actor_ui_tab_controller_;
  raw_ptr<MockActorOverlayViewController> mock_actor_overlay_view_controller_;
  raw_ptr<MockHandoffButtonController> mock_handoff_button_controller_;
};

TEST_F(ActorUiTabControllerTest, SetActorTaskStatePaused_SetsStateCorrectly) {
  actor_ui_tab_controller_->SetActorTaskPaused();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kPausedByClient);
}

TEST_F(ActorUiTabControllerTest, SetActorTaskStateResume_SetsStateCorrectly) {
  actor_ui_tab_controller_->SetActorTaskResume();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kReflecting);
}

TEST_F(ActorUiTabControllerTest, OnUiTabStateChange_NoOpIfStateIsUnchanged) {
  UiTabState ui_tab_state = UiTabState(
      ActorOverlayState(true, false, std::nullopt),
      HandoffButtonState(true, HandoffButtonState::ControlOwnership::kAgent));

  EXPECT_CALL(*mock_handoff_button_controller_, UpdateState(_, _)).Times(1);

  for (int i = 0; i < 2; i++) {
    base::RunLoop loop;
    actor_ui_tab_controller_->OnUiTabStateChange(
        ui_tab_state, base::BindLambdaForTesting([&](bool result) {
          EXPECT_TRUE(result);
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(ActorUiTabControllerTest,
       SetHandoffButtonVisibility_TrueWhenTabIsActiveAndInputIsTrue) {
  // First, ensure the tab is active.
  actor_ui_tab_controller_->OnTabActiveStatusChanged(true, &mock_tab());

  // Expect UpdateState to be called with is_visible set to true.
  EXPECT_CALL(*mock_handoff_button_controller_,
              UpdateState(HandoffButtonState(), true));

  actor_ui_tab_controller_->SetHandoffButtonVisibility(true);
}

TEST_F(ActorUiTabControllerTest,
       SetHandoffButtonVisibility_FalseWhenTabIsActiveAndInputIsFalse) {
  // First, ensure the tab is active.
  actor_ui_tab_controller_->OnTabActiveStatusChanged(true, &mock_tab());

  // Expect UpdateState to be called with is_visible set to false.
  EXPECT_CALL(*mock_handoff_button_controller_,
              UpdateState(HandoffButtonState(), false));

  actor_ui_tab_controller_->SetHandoffButtonVisibility(false);
}

TEST_F(ActorUiTabControllerTest,
       SetHandoffButtonVisibility_AlwaysFalseWhenTabIsInactive) {
  // First, ensure the tab is inactive.
  actor_ui_tab_controller_->OnTabActiveStatusChanged(false, &mock_tab());

  // Expect UpdateState to be called with is_visible set to false.
  EXPECT_CALL(*mock_handoff_button_controller_,
              UpdateState(HandoffButtonState(), false))
      .Times(2);

  actor_ui_tab_controller_->SetHandoffButtonVisibility(true);
  actor_ui_tab_controller_->SetHandoffButtonVisibility(false);
}

using UiTabStateActivationParams =
    std::tuple<bool, bool, bool>;  // <handoff_is_active,
                                   // actor_overlay_is_active, tab_is_activated>

class ActorUiTabControllerParamTest
    : public ActorUiTabControllerTest,
      public ::testing::WithParamInterface<UiTabStateActivationParams> {};

TEST_P(
    ActorUiTabControllerParamTest,
    OnTabActiveStatusChanged_CallsUiControllersWithCorrectStateAndVisibility) {
  bool handoff_is_active = std::get<0>(GetParam());
  bool actor_overlay_is_active = std::get<1>(GetParam());
  bool tab_is_activated = std::get<2>(GetParam());

  HandoffButtonState handoff_button_state(
      handoff_is_active, HandoffButtonState::ControlOwnership::kAgent);
  ActorOverlayState actor_overlay_state(actor_overlay_is_active, false,
                                        std::nullopt);
  UiTabState ui_tab_state(actor_overlay_state, handoff_button_state);

  // Set the tab's activation status and UiTabState.
  actor_ui_tab_controller_->OnTabActiveStatusChanged(!tab_is_activated,
                                                     &mock_tab());
  actor_ui_tab_controller_->OnUiTabStateChange(ui_tab_state, base::DoNothing());

  // HandoffButton visibility should always be false.
  EXPECT_CALL(*mock_handoff_button_controller_,
              UpdateState(handoff_button_state, false));
  // ActorOverlay visibility should be based on the tab's active status
  // and the actor overlay active state.
  EXPECT_CALL(*mock_actor_overlay_view_controller_,
              UpdateState(actor_overlay_state,
                          actor_overlay_is_active && tab_is_activated));
  // Simulate the tab's active status change.
  actor_ui_tab_controller_->OnTabActiveStatusChanged(tab_is_activated,
                                                     &mock_tab());
}

TEST_P(ActorUiTabControllerParamTest,
       OnUiTabStateChange_CallsUiControllersWithCorrectStateAndVisibility) {
  bool handoff_is_active = std::get<0>(GetParam());
  bool actor_overlay_is_active = std::get<1>(GetParam());
  bool tab_is_activated = std::get<2>(GetParam());

  // Set the tab's activation status and UiTabState.
  actor_ui_tab_controller_->OnTabActiveStatusChanged(tab_is_activated,
                                                     &mock_tab());
  HandoffButtonState handoff_button_state_before(
      handoff_is_active, HandoffButtonState::ControlOwnership::kAgent);
  ActorOverlayState actor_overlay_state_before(actor_overlay_is_active, false,
                                               std::nullopt);
  UiTabState ui_tab_state_before(actor_overlay_state_before,
                                 handoff_button_state_before);
  actor_ui_tab_controller_->OnUiTabStateChange(ui_tab_state_before,
                                               base::DoNothing());

  HandoffButtonState handoff_button_state_after(
      !handoff_is_active, HandoffButtonState::ControlOwnership::kAgent);
  ActorOverlayState actor_overlay_state_after(actor_overlay_is_active, false,
                                              std::nullopt);
  UiTabState ui_tab_state_after(actor_overlay_state_after,
                                handoff_button_state_after);

  // HandoffButton visibility should always be false.
  EXPECT_CALL(*mock_handoff_button_controller_,
              UpdateState(handoff_button_state_after, false));
  // ActorOverlay visibility should be based on the tab's active status
  // and the actor overlay active state.
  EXPECT_CALL(*mock_actor_overlay_view_controller_,
              UpdateState(actor_overlay_state_after,
                          actor_overlay_is_active && tab_is_activated));
  // Simulate the UiTabState change.
  actor_ui_tab_controller_->OnUiTabStateChange(ui_tab_state_after,
                                               base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(
    UiTabStateActivationCombinations,
    ActorUiTabControllerParamTest,
    ::testing::Combine(::testing::Bool(),  // handoff_is_active
                       ::testing::Bool(),  // actor_overlay_is_active
                       ::testing::Bool()   // tab_is_activated
                       ));

}  // namespace
}  // namespace actor::ui

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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
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

class MockActorUiTabControllerFactory
    : public ActorUiTabControllerFactoryInterface {
 public:
  ~MockActorUiTabControllerFactory() override {
    mock_overlay_view_controller_ = nullptr;
    mock_handoff_button_controller_ = nullptr;
  }

  std::unique_ptr<HandoffButtonController> CreateHandoffButtonController(
      tabs::TabInterface& tab) override {
    auto controller = std::make_unique<MockHandoffButtonController>(tab);
    mock_handoff_button_controller_ = controller.get();
    return controller;
  }
  std::unique_ptr<ActorOverlayViewController> CreateActorOverlayViewController(
      tabs::TabInterface& tab) override {
    auto controller = std::make_unique<MockActorOverlayViewController>(tab);
    mock_overlay_view_controller_ = controller.get();
    return controller;
  }

  MockActorOverlayViewController* overlay_controller() {
    return mock_overlay_view_controller_;
  }

  MockHandoffButtonController* handoff_button_controller() {
    return mock_handoff_button_controller_;
  }

 private:
  raw_ptr<MockActorOverlayViewController> mock_overlay_view_controller_;
  raw_ptr<MockHandoffButtonController> mock_handoff_button_controller_;
};

class ActorUiTabControllerTest : public testing::Test {
 public:
  ActorUiTabControllerTest() : tab_strip_model_(&delegate_, profile()) {}
  ~ActorUiTabControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi,
        {{features::kGlicActorUiHandoffButtonName, "true"},
         {features::kGlicActorUiOverlayName, "true"}});
    profile_ = TestingProfile::Builder().Build();

    actor_keyed_service_ = std::make_unique<ActorKeyedServiceFake>(profile());
    std::unique_ptr<MockActorUiStateManager> ausm =
        std::make_unique<MockActorUiStateManager>();
    actor_keyed_service_->SetActorUiStateManagerForTesting(std::move(ausm));
    auto controller_factory =
        std::make_unique<MockActorUiTabControllerFactory>();
    actor_ui_tab_controller_factory_ = controller_factory.get();

    ON_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(mock_browser_window_interface_, GetProfile)
        .WillByDefault(Return(profile()));
    ON_CALL(mock_browser_window_interface_, GetTabStripModel)
        .WillByDefault(Return(&tab_strip_model_));

    actor_ui_tab_controller_ = std::make_unique<ActorUiTabController>(
        mock_tab_, actor_keyed_service(), std::move(controller_factory));

    // Creates task for testing.
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

  ActorKeyedServiceFake* actor_keyed_service() {
    return actor_keyed_service_.get();
  }

  ActorUiTabControllerInterface* tab_controller() {
    return actor_ui_tab_controller_.get();
  }

  MockActorUiTabControllerFactory* tab_controller_factory() {
    return actor_ui_tab_controller_factory_;
  }

  TaskId task_id() { return task_id_; }

  TestingProfile* profile() { return profile_.get(); }

  MockTabInterface& mock_tab() { return mock_tab_; }

  void Debounce() {
    task_environment_.FastForwardBy(kUpdateStateDebounceDelay +
                                    base::Milliseconds(1));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorKeyedServiceFake> actor_keyed_service_;
  MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  TestTabStripModelDelegate delegate_;
  TabStripModel tab_strip_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TaskId task_id_;
  std::unique_ptr<ActorUiTabController> actor_ui_tab_controller_;
  raw_ptr<MockActorUiTabControllerFactory> actor_ui_tab_controller_factory_ =
      nullptr;
};

TEST_F(ActorUiTabControllerTest, SetActorTaskStatePaused_SetsStateCorrectly) {
  tab_controller()->SetActorTaskPaused();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kPausedByClient);
}

TEST_F(ActorUiTabControllerTest, SetActorTaskStateResume_SetsStateCorrectly) {
  tab_controller()->SetActorTaskResume();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kReflecting);
}

TEST_F(ActorUiTabControllerTest,
       UpdateButtonVisibility_TrueWhenTabIsActiveAndHoveringOnOverlay) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);
  tab_controller()->OnUiTabStateChange(ui_tab_state, base::DoNothing());
  tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());
  Debounce();

  // Expect UpdateState to be called with is_visible set to true.
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state, true));

  tab_controller()->SetOverlayHoverStatus(true);
  Debounce();
}

TEST_F(ActorUiTabControllerTest,
       UpdateButtonVisibility_ButtonHidesWhenHoverEnds) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);
  tab_controller()->OnUiTabStateChange(ui_tab_state, base::DoNothing());
  tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());
  Debounce();

  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(_, true))
      .Times(1);

  tab_controller()->SetOverlayHoverStatus(true);
  Debounce();
  testing::Mock::VerifyAndClearExpectations(
      tab_controller_factory()->handoff_button_controller());

  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(_, /*is_visible=*/false));

  tab_controller()->SetOverlayHoverStatus(false);
  Debounce();
}

TEST_F(ActorUiTabControllerTest,
       UpdateButtonVisibility_ButtonStaysVisibleWhenClientIsInControl) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);
  tab_controller()->OnUiTabStateChange(ui_tab_state, base::DoNothing());
  tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());
  Debounce();

  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(_, true));

  tab_controller()->SetOverlayHoverStatus(true);
  Debounce();
  testing::Mock::VerifyAndClearExpectations(
      tab_controller_factory()->handoff_button_controller());

  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(_, /*is_visible=*/true));

  // Simulate user in control.
  tab_controller()->SetOverlayHoverStatus(false);
  HandoffButtonState client_control_state(
      true, HandoffButtonState::ControlOwnership::kClient);
  UiTabState new_ui_tab_state(ActorOverlayState(), client_control_state);
  tab_controller()->OnUiTabStateChange(new_ui_tab_state, base::DoNothing());
  Debounce();
}

TEST_F(
    ActorUiTabControllerTest,
    UpdateButtonVisibility_ButtonStaysVisibleWhenHoverMovesFromOverlayToButton) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);
  tab_controller()->OnUiTabStateChange(ui_tab_state, base::DoNothing());
  tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());
  Debounce();

  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state, /*is_visible=*/true));
  tab_controller()->SetOverlayHoverStatus(true);
  Debounce();
  testing::Mock::VerifyAndClearExpectations(
      tab_controller_factory()->handoff_button_controller());

  // The mouse leaves the overlay.
  tab_controller()->SetOverlayHoverStatus(false);

  // The mouse enters the button.
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state, /*is_visible=*/true));
  tab_controller()->SetHandoffButtonHoverStatus(true);
  Debounce();

  testing::Mock::VerifyAndClearExpectations(
      tab_controller_factory()->handoff_button_controller());
}

TEST_F(ActorUiTabControllerTest,
       SetHandoffButtonHoverStatus_HoverOnButtonMakesButtonVisible) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);
  tab_controller()->OnUiTabStateChange(ui_tab_state, base::DoNothing());
  tab_controller()->OnTabActiveStatusChanged(true, &mock_tab());
  Debounce();

  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(_, /*is_visible=*/true));

  tab_controller()->SetHandoffButtonHoverStatus(true);
  Debounce();
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
      handoff_is_active, HandoffButtonState::ControlOwnership::kActor);
  ActorOverlayState actor_overlay_state(actor_overlay_is_active, false,
                                        std::nullopt);
  UiTabState ui_tab_state(actor_overlay_state, handoff_button_state);

  // Set the tab's activation status and UiTabState.
  tab_controller()->OnTabActiveStatusChanged(!tab_is_activated, &mock_tab());
  tab_controller()->OnUiTabStateChange(ui_tab_state, base::DoNothing());
  Debounce();

  // HandoffButton visibility should always be false.
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state, false));
  // ActorOverlay visibility should be based on the tab's active status
  // and the actor overlay active state.
  EXPECT_CALL(*tab_controller_factory()->overlay_controller(),
              UpdateState(actor_overlay_state,
                          actor_overlay_is_active && tab_is_activated));
  // Simulate the tab's active status change.
  tab_controller()->OnTabActiveStatusChanged(tab_is_activated, &mock_tab());
  Debounce();
}

TEST_P(ActorUiTabControllerParamTest,
       OnUiTabStateChange_CallsUiControllersWithCorrectStateAndVisibility) {
  bool handoff_is_active = std::get<0>(GetParam());
  bool actor_overlay_is_active = std::get<1>(GetParam());
  bool tab_is_activated = std::get<2>(GetParam());

  // Set the tab's activation status and UiTabState.
  tab_controller()->OnTabActiveStatusChanged(tab_is_activated, &mock_tab());
  Debounce();

  HandoffButtonState handoff_button_state_before(
      handoff_is_active, HandoffButtonState::ControlOwnership::kActor);
  ActorOverlayState actor_overlay_state_before(actor_overlay_is_active, false,
                                               std::nullopt);
  UiTabState ui_tab_state_before(actor_overlay_state_before,
                                 handoff_button_state_before);
  tab_controller()->OnUiTabStateChange(ui_tab_state_before, base::DoNothing());
  Debounce();

  HandoffButtonState handoff_button_state_after(
      !handoff_is_active, HandoffButtonState::ControlOwnership::kActor);
  ActorOverlayState actor_overlay_state_after(actor_overlay_is_active, false,
                                              std::nullopt);
  UiTabState ui_tab_state_after(actor_overlay_state_after,
                                handoff_button_state_after);

  // HandoffButton visibility should always be false.
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state_after, false));
  // ActorOverlay visibility should be based on the tab's active status
  // and the actor overlay active state.
  EXPECT_CALL(*tab_controller_factory()->overlay_controller(),
              UpdateState(actor_overlay_state_after,
                          actor_overlay_is_active && tab_is_activated));
  // Simulate the UiTabState change.
  tab_controller()->OnUiTabStateChange(ui_tab_state_after, base::DoNothing());
  Debounce();
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

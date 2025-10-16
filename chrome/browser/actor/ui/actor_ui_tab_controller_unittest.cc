// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/ui/actor_border_view_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller_factory.h"
#include "chrome/browser/actor/ui/mocks/mock_handoff_button_controller.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/mock_immersive_mode_controller.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/controls/webview/webview.h"

namespace actor::ui {
namespace {
using ::tabs::MockTabInterface;
using ::testing::_;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ReturnRef;

class MockWebContents : public content::TestWebContents {
 public:
  explicit MockWebContents(content::BrowserContext* browser_context);
  ~MockWebContents() override;

  MOCK_METHOD(base::ScopedClosureRunner,
              IncrementCapturerCount,
              (const gfx::Size&, bool, bool, bool),
              (override));
};

MockWebContents::MockWebContents(content::BrowserContext* browser_context)
    : TestWebContents(browser_context) {}

MockWebContents::~MockWebContents() = default;

ACTION(ReturnNewScopedClosureRunner) {
  return base::ScopedClosureRunner(base::DoNothing());
}

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

    ON_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile()));
    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(Return(&tab_strip_model_));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(ReturnRef(user_data_host_));

    immersive_mode_controller_ = std::make_unique<MockImmersiveModeController>(
        &mock_browser_window_interface_);
    ON_CALL(*immersive_mode_controller(), IsEnabled())
        .WillByDefault(Return(false));

    actor_keyed_service_ = std::make_unique<ActorKeyedServiceFake>(profile());
    std::unique_ptr<MockActorUiStateManager> ausm =
        std::make_unique<MockActorUiStateManager>();
    actor_keyed_service_->SetActorUiStateManagerForTesting(std::move(ausm));
    auto controller_factory =
        std::make_unique<MockActorUiTabControllerFactory>();
    actor_ui_tab_controller_factory_ = controller_factory.get();

    window_controller_ = std::make_unique<ActorUiWindowController>(
        &mock_browser_window_interface_,
        std::vector<std::pair<views::WebView*, ActorOverlayWebView*>>{});

    border_view_controller_ = std::make_unique<ActorBorderViewController>(
        &mock_browser_window_interface_);

    mock_web_contents_ = std::make_unique<MockWebContents>(profile_.get());
    ON_CALL(mock_tab_, GetContents)
        .WillByDefault(Return(mock_web_contents_.get()));
    ON_CALL(mock_tab_, IsSelected).WillByDefault(Return(true));
    ON_CALL(*mock_web_contents_, IncrementCapturerCount(_, _, _, _))
        .WillByDefault(ReturnNewScopedClosureRunner());

    actor_ui_tab_controller_ = std::make_unique<ActorUiTabController>(
        mock_tab_, actor_keyed_service(), std::move(controller_factory));

    // Creates task for testing.
    task_id_ = actor_keyed_service()->CreateTaskForTesting();
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

  ActorBorderViewController* actor_border_view_controller() {
    return border_view_controller_.get();
  }

  MockImmersiveModeController* immersive_mode_controller() {
    return immersive_mode_controller_.get();
  }

  void TearDown() override {
    window_controller_.reset();
    testing::Test::TearDown();
  }

  TaskId task_id() { return task_id_; }

  TestingProfile* profile() { return profile_.get(); }

  MockTabInterface& mock_tab() { return mock_tab_; }

  void Debounce() {
    task_environment_.FastForwardBy(kUpdateScrimBackgroundDebounceDelay +
                                    base::Milliseconds(1));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorKeyedServiceFake> actor_keyed_service_;
  ::ui::UnownedUserDataHost user_data_host_;
  MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<MockImmersiveModeController> immersive_mode_controller_;
  std::unique_ptr<ActorUiWindowController> window_controller_;
  TestTabStripModelDelegate delegate_;
  TabStripModel tab_strip_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockWebContents> mock_web_contents_;
  TaskId task_id_;
  std::unique_ptr<ActorUiTabController> actor_ui_tab_controller_;
  raw_ptr<MockActorUiTabControllerFactory> actor_ui_tab_controller_factory_ =
      nullptr;
  std::unique_ptr<ActorBorderViewController> border_view_controller_;
};

TEST_F(ActorUiTabControllerTest, SetActorTaskStatePaused_SetsStateCorrectly) {
  tab_controller()->SetActorTaskPaused();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kPausedByUser);
}

TEST_F(ActorUiTabControllerTest, SetActorTaskStateResume_SetsStateCorrectly) {
  // Must pause before resume.
  tab_controller()->SetActorTaskPaused();
  tab_controller()->SetActorTaskResume();
  EXPECT_EQ(actor_keyed_service()->GetTask(task_id())->GetState(),
            ActorTask::State::kReflecting);
}

TEST_F(ActorUiTabControllerTest,
       UpdateButtonVisibility_TrueWhenTabIsSelectedAndButtonActive) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  // Expect UpdateState to be called with is_visible set to true.
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state, true));

  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);
  tab_controller()->OnUiTabStateChange(ui_tab_state, base::DoNothing());
}

TEST_F(ActorUiTabControllerTest,
       UpdateButtonVisibility_ButtonStaysVisibleWhenClientIsInControl) {
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(_, /*is_visible=*/true));

  HandoffButtonState client_control_state(
      true, HandoffButtonState::ControlOwnership::kClient);
  UiTabState new_ui_tab_state(ActorOverlayState(), client_control_state);
  tab_controller()->OnUiTabStateChange(new_ui_tab_state, base::DoNothing());
}

TEST_F(ActorUiTabControllerTest, BorderGlowChangesOnUiTabStateChange) {
  MockFunction<void(tabs::TabInterface*, bool)> callback;
  auto subscription =
      actor_border_view_controller()->AddOnActorBorderGlowUpdatedCallback(
          base::BindRepeating(
              &testing::MockFunction<void(tabs::TabInterface*, bool)>::Call,
              base::Unretained(&callback)));

  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  ActorOverlayState actor_overlay_state{.is_active = true};
  UiTabState ui_tab_state_glow_on(actor_overlay_state, handoff_button_state,
                                  /*tab_indicator_visible=*/false,
                                  /*border_glow_visible=*/true);

  EXPECT_CALL(callback, Call(&mock_tab(), true));
  tab_controller()->OnUiTabStateChange(ui_tab_state_glow_on, base::DoNothing());

  testing::Mock::VerifyAndClearExpectations(&callback);

  UiTabState ui_tab_state_glow_off(actor_overlay_state, handoff_button_state,
                                   /*tab_indicator_visible=*/false,
                                   /*border_glow_visible=*/false);
  EXPECT_CALL(callback, Call(&mock_tab(), false));
  tab_controller()->OnUiTabStateChange(ui_tab_state_glow_off,
                                       base::DoNothing());

  testing::Mock::VerifyAndClearExpectations(&callback);

  // Test that the glow is not shown when the tab is not selected.
  ON_CALL(mock_tab(), IsSelected).WillByDefault(Return(false));
  EXPECT_CALL(callback, Call(&mock_tab(), false));
  tab_controller()->OnUiTabStateChange(ui_tab_state_glow_on, base::DoNothing());
}

TEST_F(ActorUiTabControllerTest, HandoffButtonHidesWhenInImmersiveMode) {
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(_, /*is_visible=*/false));
  ON_CALL(*immersive_mode_controller(), IsEnabled())
      .WillByDefault(Return(true));
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);
  base::test::TestFuture<bool> future;
  tab_controller()->OnUiTabStateChange(ui_tab_state, future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ActorUiTabControllerTest,
       OnUiTabStateChange_SameStateRunsCallbackOnceAndDoesNotUpdateState) {
  ActorOverlayState actor_overlay_state{.is_active = true};
  HandoffButtonState handoff_button_state(
      /*is_active=*/true,
      /*control_ownership=*/HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(actor_overlay_state, handoff_button_state);

  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state, /*is_visible=*/true));

  base::test::TestFuture<bool> future1;
  tab_controller()->OnUiTabStateChange(ui_tab_state, future1.GetCallback());
  EXPECT_TRUE(future1.Get());

  // On second call, the callback should be run and the state shouldn't be
  // updated.
  EXPECT_CALL(*tab_controller_factory()->handoff_button_controller(),
              UpdateState(handoff_button_state, /*is_visible=*/true))
      .Times(0);

  base::test::TestFuture<bool> future2;
  tab_controller()->OnUiTabStateChange(ui_tab_state, future2.GetCallback());
  EXPECT_TRUE(future2.Get());
}

TEST_F(ActorUiTabControllerTest, OnUiTabStateChange_CallsCallbacks) {
  HandoffButtonState handoff_button_state(
      true, HandoffButtonState::ControlOwnership::kActor);
  UiTabState ui_tab_state(ActorOverlayState(), handoff_button_state);

  base::test::TestFuture<bool> future1;
  tab_controller()->OnUiTabStateChange(ui_tab_state, future1.GetCallback());
  EXPECT_TRUE(future1.Get());

  // Creates a new state to trigger another update ui call.
  handoff_button_state.is_active = false;
  UiTabState ui_tab_state1(ActorOverlayState(), handoff_button_state);

  base::test::TestFuture<bool> future2;
  tab_controller()->OnUiTabStateChange(ui_tab_state1, future2.GetCallback());
  EXPECT_TRUE(future2.Get());

  handoff_button_state.is_active = true;
  UiTabState ui_tab_state2(ActorOverlayState(), handoff_button_state);
  tab_controller()->OnUiTabStateChange(ui_tab_state2,
                                       base::OnceCallback<void(bool)>());
}

TEST_F(ActorUiTabControllerTest, SetScrimBackgroundOnHoverChanges) {
  int callback_count = 0;
  auto* mock_handoff_button_controller =
      tab_controller_factory()->handoff_button_controller();

  std::vector<base::CallbackListSubscription> subscriptions;
  ON_CALL(*mock_handoff_button_controller, IsHovering())
      .WillByDefault(Return(false));
  subscriptions.push_back(
      tab_controller()->RegisterActorOverlayBackgroundChange(
          base::BindLambdaForTesting([&callback_count](bool is_visible) {
            callback_count++;
            EXPECT_TRUE(is_visible);
          })));
  tab_controller()->OnOverlayHoverStatusChanged(/*is_hovering=*/true);
  Debounce();
  EXPECT_EQ(callback_count, 1);

  ON_CALL(*mock_handoff_button_controller, IsHovering())
      .WillByDefault(Return(true));
  tab_controller()->OnHandoffButtonHoverStatusChanged();
  tab_controller()->OnOverlayHoverStatusChanged(/*is_hovering=*/true);
  Debounce();
  EXPECT_EQ(callback_count, 1);

  ON_CALL(*mock_handoff_button_controller, IsHovering())
      .WillByDefault(Return(true));
  tab_controller()->OnOverlayHoverStatusChanged(/*is_hovering=*/false);
  Debounce();
  EXPECT_EQ(callback_count, 1);
  subscriptions.clear();
  callback_count = 0;

  ON_CALL(*mock_handoff_button_controller, IsHovering())
      .WillByDefault(Return(false));
  subscriptions.push_back(
      tab_controller()->RegisterActorOverlayBackgroundChange(
          base::BindLambdaForTesting([&callback_count](bool is_visible) {
            callback_count++;
            EXPECT_FALSE(is_visible);
          })));
  tab_controller()->OnHandoffButtonHoverStatusChanged();
  tab_controller()->OnOverlayHoverStatusChanged(/*is_hovering=*/false);
  Debounce();
  EXPECT_EQ(callback_count, 1);

  ON_CALL(*mock_handoff_button_controller, IsHovering())
      .WillByDefault(Return(false));
  tab_controller()->OnOverlayHoverStatusChanged(/*is_hovering=*/false);
  tab_controller()->OnHandoffButtonHoverStatusChanged();
  Debounce();
  EXPECT_EQ(callback_count, 1);
  subscriptions.clear();
}

TEST_F(ActorUiTabControllerTest, From_RecordsHistogramWhenTabDoesNotExist) {
  base::HistogramTester histogram_tester;
  ActorUiTabControllerInterface::From(nullptr);
  histogram_tester.ExpectBucketCount(
      "Actor.Ui.TabController.Error",
      ActorUiTabControllerError::kRequestedForNonExistentTab, 1);
}
}  // namespace
}  // namespace actor::ui

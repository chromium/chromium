// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace actor::ui {
namespace {

using ::actor::mojom::ActionResultPtr;
using ::tabs::MockTabInterface;
using ::tabs::TabFeatures;
using ::tabs::TabInterface;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ValuesIn;

using enum HandoffButtonState::ControlOwnership;

class ActorUiStateManagerTest : public testing::Test {
 public:
  ActorUiStateManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ActorUiStateManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActorUi,
                              features::kGlicHandoffButtonHiddenClientControl},
        /*disabled_features=*/{});
    profile_ = TestingProfile::Builder()
                   .AddTestingFactory(
                       ActorKeyedServiceFactory::GetInstance(),
                       base::BindRepeating(
                           &ActorUiStateManagerTest::BuildActorKeyedService,
                           base::Unretained(this)))
                   .Build();

    test_tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &test_tab_strip_model_delegate_, profile());
    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));

    MockActorUiTabController::SetupDefaultBrowserWindow(
        mock_tab(), mock_browser_window_interface_, user_data_host_);
    mock_actor_ui_tab_controller_.emplace(mock_tab());
  }

  std::unique_ptr<KeyedService> BuildActorKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto actor_keyed_service = std::make_unique<ActorKeyedServiceFake>(profile);
    actor_keyed_service_fake_ = actor_keyed_service.get();

    return std::move(actor_keyed_service);
  }

  void OnUiEventComplete(AsyncUiEvent event) {
    base::RunLoop loop;
    actor_ui_state_manager()->OnUiEvent(
        event, base::BindLambdaForTesting([&](ActionResultPtr result) {
          EXPECT_TRUE(IsOk(*result));
          loop.Quit();
        }));
    loop.Run();
  }

  void ExpectUiTabStateChange(const UiTabState& expected_state) {
    ON_CALL(*mock_actor_ui_tab_controller(), OnUiTabStateChange)
        .WillByDefault(
            [&](UiTabState state, base::OnceCallback<void(bool)> callback) {
              EXPECT_EQ(state, expected_state);
              std::move(callback).Run(true);
            });
  }

  ActorUiStateManagerInterface* actor_ui_state_manager() {
    return actor_keyed_service_fake_->GetActorUiStateManager();
  }

  ActorKeyedServiceFake* actor_keyed_service() {
    return actor_keyed_service_fake_;
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  TestingProfile* profile() { return profile_.get(); }

  MockTabInterface& mock_tab() { return mock_tab_; }

  MockActorUiTabController* mock_actor_ui_tab_controller() {
    return &mock_actor_ui_tab_controller_.value();
  }

  // TODO(crbug.com/424495020): Refactor the actor_keyed_service_fake to remove
  // manual setting of task states in the below tests.
  void PauseActorTask(TaskId task_id, bool from_actor) {
    actor_keyed_service()->GetTask(task_id)->Pause(from_actor);
    if (from_actor) {
      actor_ui_state_manager()->OnUiEvent(TaskStateChanged(
          task_id, ActorTask::State::kPausedByActor, /*title=*/""));
    } else {
      actor_ui_state_manager()->OnUiEvent(TaskStateChanged(
          task_id, ActorTask::State::kPausedByUser, /*title=*/""));
    }
  }

  void ResumeActorTask(TaskId task_id) {
    actor_keyed_service()->GetTask(task_id)->Resume();
    TaskStateChanged reflecting_task_event(
        task_id, ActorTask::State::kReflecting, /*title=*/"");
    actor_ui_state_manager()->OnUiEvent(reflecting_task_event);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ActorKeyedServiceFake> actor_keyed_service_fake_;
  ::ui::UnownedUserDataHost user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  MockTabInterface mock_tab_;
  TestTabStripModelDelegate test_tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::optional<MockActorUiTabController> mock_actor_ui_tab_controller_;
};

TEST_F(ActorUiStateManagerTest, SingleTask_RapidTaskStateChanges_Debounced) {
  std::vector<base::CallbackListSubscription> subscriptions;
  int callback_count = 0;

  subscriptions.push_back(
      actor_ui_state_manager()->RegisterActorTaskStateChange(
          base::BindLambdaForTesting(
              [&callback_count](TaskId task_id) { callback_count++; })));

  // 1. Create a task.
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event);

  // Immediately pause and resume without waiting for the debounce delay.
  PauseActorTask(task_id, /*from_actor=*/true);
  ResumeActorTask(task_id);

  EXPECT_EQ(callback_count, 0);

  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(callback_count, 1);
}

TEST_F(ActorUiStateManagerTest, OnActorTaskState_kCreatedNewStateCrashes) {
  EXPECT_DEATH(actor_ui_state_manager()->OnUiEvent(TaskStateChanged(
                   TaskId(123), ActorTask::State::kCreated, /*title=*/"")),
               "");
}

class ActorUiStateManagerActorTaskUiTabScopedTest
    : public ActorUiStateManagerTest,
      public testing::WithParamInterface<
          std::tuple<ActorTask::State, UiTabState>> {
 public:
  void SetUp() override { ActorUiStateManagerTest::SetUp(); }
};

TEST_P(ActorUiStateManagerActorTaskUiTabScopedTest,
       OnActorTaskState_UpdateTabScopedUi) {
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();

  base::RunLoop loop;
  actor_keyed_service()->GetTask(task_id)->AddTab(
      mock_tab().GetHandle(),
      base::BindLambdaForTesting([&](ActionResultPtr result) {
        EXPECT_TRUE(IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  auto [task_state, expected_ui_tab_state] = GetParam();
  ExpectUiTabStateChange(expected_ui_tab_state);
  actor_ui_state_manager()->OnUiEvent(
      TaskStateChanged(task_id, task_state, /*title=*/""));
}

const auto kActorTaskTestValues = std::vector<
    std::tuple<ActorTask::State, UiTabState>>{
    {ActorTask::State::kActing,
     UiTabState{
         .actor_overlay = {.is_active = true, .border_glow_visible = true},
         .handoff_button = {.is_active = true, .controller = kActor},
         .tab_indicator = TabIndicatorStatus::kDynamic,
         .border_glow_visible = true,
     }},
    {ActorTask::State::kReflecting,
     UiTabState{
         .actor_overlay = {.is_active = true, .border_glow_visible = true},
         .handoff_button = {.is_active = true, .controller = kActor},
         .tab_indicator = TabIndicatorStatus::kDynamic,
         .border_glow_visible = true,
     }},
    {ActorTask::State::kWaitingOnUser,
     UiTabState{
         .actor_overlay = {.is_active = true, .border_glow_visible = true},
         .handoff_button = {.is_active = true, .controller = kActor},
         .tab_indicator = TabIndicatorStatus::kStatic,
         .border_glow_visible = true,
     }},
    {ActorTask::State::kPausedByActor,
     UiTabState{
         .actor_overlay = {.is_active = false, .border_glow_visible = false},
         .handoff_button = {.is_active = false, .controller = kClient},
         .tab_indicator = TabIndicatorStatus::kNone,
         .border_glow_visible = false,
     }},
    {ActorTask::State::kPausedByUser,
     UiTabState{
         .actor_overlay = {.is_active = false, .border_glow_visible = false},
         .handoff_button = {.is_active = false, .controller = kClient},
         .tab_indicator = TabIndicatorStatus::kNone,
         .border_glow_visible = false,
     }},
    {ActorTask::State::kCancelled,
     UiTabState{
         .actor_overlay = {.is_active = false, .border_glow_visible = false},
         .handoff_button = {.is_active = false},
         .tab_indicator = TabIndicatorStatus::kNone,
     }},
    {ActorTask::State::kFailed,
     UiTabState{
         .actor_overlay = {.is_active = false, .border_glow_visible = false},
         .handoff_button = {.is_active = false},
         .tab_indicator = TabIndicatorStatus::kNone,
     }},
    {ActorTask::State::kFinished,
     UiTabState{
         .actor_overlay = {.is_active = false, .border_glow_visible = false},
         .handoff_button = {.is_active = false},
         .tab_indicator = TabIndicatorStatus::kNone,
         .border_glow_visible = false,
     }}};

INSTANTIATE_TEST_SUITE_P(ActorUiStateManagerActorTaskUiTabScopedTest,
                         ActorUiStateManagerActorTaskUiTabScopedTest,
                         ValuesIn(kActorTaskTestValues));

class ActorUiStateManagerUiEventUiTabScopedTest
    : public ActorUiStateManagerTest {
 public:
  void SetUp() override { ActorUiStateManagerTest::SetUp(); }

  void VerifyUiEvent(AsyncUiEvent event, UiTabState expected_state) {
    ExpectUiTabStateChange(expected_state);
    OnUiEventComplete(event);
  }

  void VerifyUiEvent(SyncUiEvent event, UiTabState expected_state) {
    ExpectUiTabStateChange(expected_state);
    actor_ui_state_manager()->OnUiEvent(event);
  }
};

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnStartingToActOnTab_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .actor_overlay = {.is_active = true, .border_glow_visible = true},
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator = TabIndicatorStatus::kDynamic,
      .border_glow_visible = true,
  };
  VerifyUiEvent(StartingToActOnTab{mock_tab().GetHandle(), TaskId(123)},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnStoppedActingOnTab_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .actor_overlay = {.is_active = false},
      .handoff_button = {.is_active = false},
      .tab_indicator = TabIndicatorStatus::kNone,
      .border_glow_visible = false,
  };
  VerifyUiEvent(StoppedActingOnTab{mock_tab().GetHandle()},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnMouseMove_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .actor_overlay = {.is_active = true,
                        .border_glow_visible = true,
                        .mouse_down = false,
                        .mouse_target = gfx::Point(100, 200)},
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator = TabIndicatorStatus::kDynamic,
      .border_glow_visible = true,
  };
  VerifyUiEvent(MouseMove{mock_tab().GetHandle(), gfx::Point(100, 200),
                          TargetSource::kToolRequest},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnMouseClick_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .actor_overlay = {.is_active = true,
                        .border_glow_visible = true,
                        .mouse_down = true},
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator = TabIndicatorStatus::kDynamic,
      .border_glow_visible = true,
  };
  VerifyUiEvent(MouseClick{mock_tab().GetHandle(), MouseClickType::kLeft,
                           MouseClickCount::kSingle},
                expected_ui_tab_state);
}

}  // namespace
}  // namespace actor::ui

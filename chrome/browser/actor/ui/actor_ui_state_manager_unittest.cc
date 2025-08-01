// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/ui/mock_actor_ui_tab_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {

using ::actor::mojom::ActionResultPtr;
using ::tabs::MockTabInterface;
using ::tabs::TabFeatures;
using ::tabs::TabInterface;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ValuesIn;

using enum HandoffButtonState::ControlOwnership;

class ActorUiStateManagerFake : public ActorUiStateManager {
 public:
  explicit ActorUiStateManagerFake(ActorKeyedService& actor_service)
      : ActorUiStateManager(actor_service) {
    mock_tab_controller_ = std::make_unique<MockActorUiTabController>();
    ON_CALL(*mock_tab_controller_, OnUiTabStateChange(_, _))
        .WillByDefault(Invoke(
            [this](UiTabState state, base::OnceCallback<void(bool)> callback) {
              this->SetUiTabState(state, std::move(callback));
            }));
  }

  ActorUiTabControllerInterface* GetUiTabController(
      tabs::TabInterface* tab) override {
    return mock_tab_controller_.get();
  }

  void SetUiTabState(UiTabState ui_tab_state,
                     base::OnceCallback<void(bool)> callback) {
    ui_tab_state_ = ui_tab_state;
    std::move(callback).Run(true);
  }

  UiTabState GetUiTabState() { return ui_tab_state_; }

  void SetUiStateForTesting(UiState new_state) { state_ = new_state; }

 private:
  UiTabState ui_tab_state_;
  std::unique_ptr<MockActorUiTabController> mock_tab_controller_;
};

class ActorUiStateManagerTest : public testing::Test {
 public:
  ActorUiStateManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ActorUiStateManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActorUi},
        /*disabled_features=*/{});
    profile_ = TestingProfile::Builder()
                   .AddTestingFactory(
                       ActorKeyedServiceFactory::GetInstance(),
                       base::BindRepeating(
                           &ActorUiStateManagerTest::BuildActorKeyedService,
                           base::Unretained(this)))
                   .Build();
  }

  std::unique_ptr<KeyedService> BuildActorKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto actor_keyed_service = std::make_unique<ActorKeyedServiceFake>(profile);
    actor_keyed_service_fake_ = actor_keyed_service.get();

    auto actor_ui_state_manager_fake =
        std::make_unique<ActorUiStateManagerFake>(*actor_keyed_service);
    actor_ui_state_manager_fake_ = actor_ui_state_manager_fake.get();
    actor_keyed_service->SetActorUiStateManagerForTesting(
        std::move(actor_ui_state_manager_fake));
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

  ActorUiStateManagerFake* actor_ui_state_manager() {
    return actor_ui_state_manager_fake_;
  }

  ActorKeyedServiceFake* actor_keyed_service() {
    return actor_keyed_service_fake_;
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  TestingProfile* profile() { return profile_.get(); }

  // TODO(crbug.com/424495020): Refactor the actor_keyed_service_fake to set
  // Active/Inactive tasks correct from ActorTask states and then remove manual
  // setting of task states in the below tests.
  void PauseActorTask(TaskId task_id) {
    actor_keyed_service()->GetTask(task_id)->Pause();
    TaskStateChanged pause_task_event(task_id,
                                      ActorTask::State::kPausedByClient);
    actor_ui_state_manager()->OnUiEvent(pause_task_event);
  }

  void ResumeActorTask(TaskId task_id) {
    actor_keyed_service()->GetTask(task_id)->Resume();
    TaskStateChanged reflecting_task_event(task_id,
                                           ActorTask::State::kReflecting);
    actor_ui_state_manager()->OnUiEvent(reflecting_task_event);
  }

  void StopActorTask(TaskId task_id) {
    actor_keyed_service()->StopTask(task_id);
    TaskStateChanged finished_task_event(task_id, ActorTask::State::kFinished);
    actor_ui_state_manager()->OnUiEvent(finished_task_event);
  }

  MockBrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ActorKeyedServiceFake> actor_keyed_service_fake_;
  raw_ptr<ActorUiStateManagerFake> actor_ui_state_manager_fake_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
};

#if BUILDFLAG(ENABLE_GLIC)
TEST_F(ActorUiStateManagerTest, GlicUpdateFloatyState_NotifiesSubscribers) {
  std::vector<base::CallbackListSubscription> subscriptions;
  actor_ui_state_manager()->SetUiStateForTesting(
      ActorUiStateManager::UiState::kCheckTasks);
  subscriptions.push_back(
      actor_ui_state_manager()->RegisterFloatyTaskStateChange(
          base::BindRepeating(
              [](ActorUiStateManager::UiState actual_ui_state,
                 glic::GlicWindowController::State actual_glic_state) {
                EXPECT_EQ(actual_ui_state,
                          ActorUiStateManager::UiState::kCheckTasks);
                EXPECT_EQ(actual_glic_state,
                          glic::GlicWindowController::State::kOpen);
              })));
  actor_ui_state_manager()->OnGlicUpdateFloatyState(
      glic::GlicWindowController::State::kOpen, browser_window_interface());
}

#endif

TEST_F(ActorUiStateManagerTest, NoTask_ReturnsInactiveUiState) {
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kInactive);
}

TEST_F(ActorUiStateManagerTest, SingleTask_ReturnsCorrectUiState) {
  // Create a task.
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Pause the task.
  PauseActorTask(task_id);
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Resume the task.
  ResumeActorTask(task_id);
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Stop the task.
  StopActorTask(task_id);
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);
  task_environment().FastForwardBy(kCompletedTaskExpiryDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kInactive);
}

TEST_F(ActorUiStateManagerTest, SingleTask_RapidStateChanges_Debounced) {
  // 1. Create a task.
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event);

  // Immediately pause and resume without waiting for the debounce delay.
  PauseActorTask(task_id);
  ResumeActorTask(task_id);

  // The debounce delay timer has not yet fired so we should still be in the
  // active state.
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // The last action was resuming, so we should never be in the kCheckTasks
  // state.
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);
}

TEST_F(ActorUiStateManagerTest, MultiTask_OneTaskPaused_ReturnsCorrectUiState) {
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Pause the first task, the state should now be in kCheckTasks.
  PauseActorTask(task_id);
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Create another task, the state should still be in kCheckTasks.
  TaskId task_id2 = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event2(task_id2);
  MockTabInterface mock_tab2;
  actor_ui_state_manager()->OnUiEvent(start_task_event2);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Resume the first task, the state should now be in kActive.
  ResumeActorTask(task_id);
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);
}

TEST_F(ActorUiStateManagerTest,
       MultiTask_OneTaskComplete_ReturnsCorrectUiState) {
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Stop first task.
  StopActorTask(task_id);
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Create another task.
  TaskId task_id2 = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event2(task_id2);
  actor_ui_state_manager()->OnUiEvent(start_task_event2);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // The state should still be active due to task2 after the expiry period.
  task_environment().FastForwardBy(kCompletedTaskExpiryDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // When both tasks stop, then the state should be inactive.
  StopActorTask(task_id2);
  task_environment().FastForwardBy(kCompletedTaskExpiryDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kInactive);
}

TEST_F(ActorUiStateManagerTest,
       MultiTask_MultipleTasksComplete_ReturnsCorrectUiState) {
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Create another task.
  TaskId task_id2 = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event2(task_id2);
  actor_ui_state_manager()->OnUiEvent(start_task_event2);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Stop both tasks within delay of each other.
  base::Time task1_finish_time = base::Time::Now();
  StopActorTask(task_id);
  task_environment().FastForwardBy(base::Minutes(1));
  StopActorTask(task_id2);

  base::TimeDelta delay =
      kCompletedTaskExpiryDelay - (base::Time::Now() - task1_finish_time);
  task_environment().FastForwardBy((delay.is_positive()) ? delay
                                                         : base::TimeDelta());
  // Even though the first task expired, we should still be in the correct
  // state.
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // After both tasks expire, the state should be inactive.
  task_environment().FastForwardBy(kCompletedTaskExpiryDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kInactive);
}

TEST_F(ActorUiStateManagerTest, OnActorTaskState_kCreatedNewStateCrashes) {
  EXPECT_DEATH(actor_ui_state_manager()->OnUiEvent(
                   TaskStateChanged(TaskId(123), ActorTask::State::kCreated)),
               "");
}

class ActorUiStateManagerActorTaskUiTabScopedTest
    : public ActorUiStateManagerTest,
      public testing::WithParamInterface<
          std::tuple<ActorTask::State, UiTabState>> {};

TEST_P(ActorUiStateManagerActorTaskUiTabScopedTest,
       OnActorTaskState_UpdateTabScopedUi) {
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  MockTabInterface mock_tab;

  base::RunLoop loop;
  actor_keyed_service()->GetTask(task_id)->AddTab(
      mock_tab.GetHandle(),
      base::BindLambdaForTesting([&](ActionResultPtr result) {
        EXPECT_TRUE(IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  auto [task_state, expected_ui_tab_state] = GetParam();
  actor_ui_state_manager()->OnUiEvent(TaskStateChanged(task_id, task_state));
  EXPECT_EQ(actor_ui_state_manager()->GetUiTabState(), expected_ui_tab_state);
}

const auto kActorTaskTestValues =
    std::vector<std::tuple<ActorTask::State, UiTabState>>{
        {ActorTask::State::kActing,
         UiTabState{
             .actor_overlay = ActorOverlayState(/*is_active=*/true),
             .handoff_button = {.is_active = true, .controller = kActor},
             .tab_indicator_visible = true,
         }},
        {ActorTask::State::kReflecting,
         UiTabState{
             .actor_overlay = ActorOverlayState(/*is_active=*/true),
             .handoff_button = {.is_active = true, .controller = kActor},
             .tab_indicator_visible = true,
         }},
        {ActorTask::State::kPausedByClient,
         UiTabState{
             .actor_overlay = ActorOverlayState(/*is_active=*/false),
             .handoff_button = {.is_active = true, .controller = kClient},
             .tab_indicator_visible = false,
         }},
        {ActorTask::State::kFinished,
         UiTabState{
             .actor_overlay = ActorOverlayState(/*is_active=*/false),
             .handoff_button = {.is_active = false},
             .tab_indicator_visible = false,
         }}};

INSTANTIATE_TEST_SUITE_P(ActorUiStateManagerActorTaskUiTabScopedTest,
                         ActorUiStateManagerActorTaskUiTabScopedTest,
                         ValuesIn(kActorTaskTestValues));

class ActorUiStateManagerUiEventUiTabScopedTest
    : public ActorUiStateManagerTest {
 public:
  void VerifyUiEvent(AsyncUiEvent event, UiTabState expected_state) {
    OnUiEventComplete(event);
    EXPECT_EQ(actor_ui_state_manager()->GetUiTabState(), expected_state);
  }

  void VerifyUiEvent(SyncUiEvent event, UiTabState expected_state) {
    actor_ui_state_manager()->OnUiEvent(event);
    EXPECT_EQ(actor_ui_state_manager()->GetUiTabState(), expected_state);
  }

 protected:
  MockTabInterface mock_tab_;
};

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnStartingToActOnTab_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .actor_overlay = ActorOverlayState(/*is_active=*/true),
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator_visible = true};
  VerifyUiEvent(StartingToActOnTab{mock_tab_.GetHandle(), TaskId(123)},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnStoppedActingOnTab_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .actor_overlay = ActorOverlayState(/*is_active=*/false),
      .handoff_button = {.is_active = false},
      .tab_indicator_visible = false,
  };
  VerifyUiEvent(StoppedActingOnTab{mock_tab_.GetHandle()},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnMouseMove_UpdatesUiCorrectly) {
  PageTarget page_target(gfx::Point(100, 200));
  UiTabState expected_ui_tab_state{
      .actor_overlay = ActorOverlayState(
          /*is_active=*/true, /*mouse_down=*/false, page_target),
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator_visible = true,
  };
  VerifyUiEvent(MouseMove{mock_tab_.GetHandle(), page_target},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnMouseClick_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .actor_overlay =
          ActorOverlayState(/*is_active=*/true, /*mouse_down=*/true),
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator_visible = true,
  };
  VerifyUiEvent(MouseClick{mock_tab_.GetHandle(), MouseClickType::kLeft,
                           MouseClickCount::kSingle},
                expected_ui_tab_state);
}

}  // namespace
}  // namespace actor::ui

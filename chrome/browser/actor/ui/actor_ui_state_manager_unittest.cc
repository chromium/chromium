// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/mock_actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/mock_event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {
using ::tabs::MockTabInterface;
using ::tabs::TabFeatures;
using ::tabs::TabInterface;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ValuesIn;
using ui::MockUiEventDispatcher;
using ui::UiEventDispatcher;
class ActorUiStateManagerFake : public ActorUiStateManager {
 public:
  explicit ActorUiStateManagerFake(ActorKeyedService& actor_service)
      : ActorUiStateManager(actor_service) {
    mock_tab_controller_ = std::make_unique<MockActorUiTabController>();
    ON_CALL(*mock_tab_controller_, OnUiTabStateChange(_))
        .WillByDefault(
            Invoke([this](UiTabState state) { this->SetUiTabState(state); }));
  }

  void RunOnUiTabController(tabs::TabInterface* tab,
                            ActorUiTabControllerCallback callback) override {
    std::move(callback).Run(*mock_tab_controller_);
  }

  void SetUiTabState(UiTabState ui_tab_state) { ui_tab_state_ = ui_tab_state; }

  UiTabState GetUiTabState() { return ui_tab_state_; }

 private:
  UiTabState ui_tab_state_;
  std::unique_ptr<MockActorUiTabController> mock_tab_controller_;
};

class ActorKeyedServiceFake : public ActorKeyedService {
 public:
  explicit ActorKeyedServiceFake(Profile* profile)
      : ActorKeyedService(profile) {}

  TaskId CreateTaskForTesting() {
    std::unique_ptr<UiEventDispatcher> ui_event_dispatcher =
        ui::NewMockUiEventDispatcher();
    auto execution_engine = ExecutionEngine::CreateForTesting(
        GetProfile(), std::move(ui_event_dispatcher));
    auto actor_task =
        std::make_unique<ActorTask>(GetProfile(), std::move(execution_engine));
    return AddActiveTask(std::move(actor_task));
  }

 private:
  base::WeakPtrFactory<ActorKeyedServiceFake> weak_ptr_factory_{this};
};

class ActorUiStateManagerTest : public testing::Test {
 public:
  ActorUiStateManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ActorUiStateManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
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
    auto actor_keyed_service =
        std::make_unique<ActorKeyedServiceFake>(static_cast<Profile*>(context));
    std::unique_ptr<ActorUiStateManagerInterface> actor_ui_state_manager_fake =
        std::make_unique<ActorUiStateManagerFake>(*actor_keyed_service);
    actor_keyed_service->SetActorUiStateManagerForTesting(
        std::move(actor_ui_state_manager_fake));
    return std::move(actor_keyed_service);
  }

  ActorUiStateManagerFake* actor_ui_state_manager() {
    return static_cast<ActorUiStateManagerFake*>(
        ActorKeyedService::Get(profile())->GetActorUiStateManager());
  }

  ActorKeyedServiceFake* actor_keyed_service() {
    return static_cast<ActorKeyedServiceFake*>(
        ActorKeyedService::Get(profile()));
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ActorUiStateManagerTest, NoTask_ReturnsInactiveUiState) {
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kInactive);
}

TEST_F(ActorUiStateManagerTest, SingleTask_ReturnsCorrectUiState) {
  // Create a task.
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event, base::DoNothing());
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Pause the task.
  actor_keyed_service()->GetTask(task_id)->Pause();
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Resume the task.
  actor_keyed_service()->GetTask(task_id)->Resume();
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Stop the task.
  actor_keyed_service()->StopTask(task_id);
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
  actor_ui_state_manager()->OnUiEvent(start_task_event, base::DoNothing());

  // Immediately pause and resume without waiting for the debounce delay.
  actor_keyed_service()->GetTask(task_id)->Pause();
  actor_keyed_service()->GetTask(task_id)->Resume();

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
  actor_ui_state_manager()->OnUiEvent(start_task_event, base::DoNothing());
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Pause the first task, the state should now be in kCheckTasks.
  actor_keyed_service()->GetTask(task_id)->Pause();
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Create another task, the state should still be in kCheckTasks.
  TaskId task_id2 = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event2(task_id2);
  MockTabInterface mock_tab2;
  actor_ui_state_manager()->OnUiEvent(start_task_event2, base::DoNothing());
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Resume the first task, the state should now be in kActive.
  actor_keyed_service()->GetTask(task_id)->Resume();
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);
}

TEST_F(ActorUiStateManagerTest,
       MultiTask_OneTaskComplete_ReturnsCorrectUiState) {
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event, base::DoNothing());
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Stop first task.
  actor_keyed_service()->StopTask(task_id);
  task_environment().FastForwardBy(kProfileScopedUiUpdateDebounceDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // Create another task.
  TaskId task_id2 = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event2(task_id2);
  actor_ui_state_manager()->OnUiEvent(start_task_event2, base::DoNothing());
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kCheckTasks);

  // The state should still be active due to task2 after the expiry period.
  task_environment().FastForwardBy(kCompletedTaskExpiryDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // When both tasks stop, then the state should be inactive.
  actor_keyed_service()->StopTask(task_id2);
  task_environment().FastForwardBy(kCompletedTaskExpiryDelay);
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kInactive);
}

TEST_F(ActorUiStateManagerTest,
       MultiTask_MultipleTasksComplete_ReturnsCorrectUiState) {
  TaskId task_id = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event(task_id);
  actor_ui_state_manager()->OnUiEvent(start_task_event, base::DoNothing());
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Create another task.
  TaskId task_id2 = actor_keyed_service()->CreateTaskForTesting();
  StartTask start_task_event2(task_id2);
  actor_ui_state_manager()->OnUiEvent(start_task_event2, base::DoNothing());
  EXPECT_EQ(actor_ui_state_manager()->GetUiState(),
            ActorUiStateManager::UiState::kActive);

  // Stop both tasks within delay of each other.
  base::Time task1_finish_time = base::Time::Now();
  actor_keyed_service()->StopTask(task_id);
  task_environment().FastForwardBy(base::Minutes(1));
  actor_keyed_service()->StopTask(task_id2);

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
  actor_keyed_service()->GetTask(task_id)->AddToTabSet(mock_tab.GetHandle());
  auto [task_state, expected_ui_tab_state] = GetParam();
  actor_ui_state_manager()->OnUiEvent(TaskStateChanged(task_id, task_state));
  EXPECT_EQ(actor_ui_state_manager()->GetUiTabState(), expected_ui_tab_state);
}

const auto kActorTaskTestValues = std::vector<
    std::tuple<ActorTask::State, UiTabState>>{
    {ActorTask::State::kActing,
     UiTabState{
         .agent_overlay = AgentOverlayState(/*is_active=*/true),
         .handoff_button = {.is_active = true,
                            .controller =
                                HandoffButtonState::ControlOwnership::kAgent}}},
    {ActorTask::State::kReflecting,
     UiTabState{
         .agent_overlay = AgentOverlayState(/*is_active=*/true),
         .handoff_button = {.is_active = true,
                            .controller =
                                HandoffButtonState::ControlOwnership::kAgent}}},
    {ActorTask::State::kPausedByClient,
     UiTabState{
         .agent_overlay = AgentOverlayState(/*is_active=*/false),
         .handoff_button =
             {.is_active = true,
              .controller = HandoffButtonState::ControlOwnership::kClient}}},
    {ActorTask::State::kFinished,
     UiTabState{.agent_overlay = AgentOverlayState(/*is_active=*/false),
                .handoff_button = {.is_active = false}}}};

INSTANTIATE_TEST_SUITE_P(ActorUiStateManagerActorTaskUiTabScopedTest,
                         ActorUiStateManagerActorTaskUiTabScopedTest,
                         ValuesIn(kActorTaskTestValues));

class ActorUiStateManagerUiEventUiTabScopedTest
    : public ActorUiStateManagerTest {
 public:
  // TODO(crbug.com/424495020): Once a callback is added from tabcontroller, add
  // RunLoop impl.
  // The state setting portion of `OnUiEvent` is synchronous and the result is
  // set immediately. The completion callback is posted and
  // can be ignored for now.
  void VerifyUiEvent(AsyncUiEvent event, UiTabState expected_state) {
    actor_ui_state_manager()->OnUiEvent(event, base::DoNothing());
    EXPECT_EQ(actor_ui_state_manager()->GetUiTabState(), expected_state);
  }

 protected:
  MockTabInterface mock_tab_;
};

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnStartingToActOnTab_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .agent_overlay = AgentOverlayState(/*is_active=*/true),
      .handoff_button = {
          .is_active = true,
          .controller = HandoffButtonState::ControlOwnership::kAgent}};
  VerifyUiEvent(StartingToActOnTab{mock_tab_.GetHandle(), TaskId(123)},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnStoppedActingOnTab_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .agent_overlay = AgentOverlayState(/*is_active=*/false),
      .handoff_button = {.is_active = false}};
  VerifyUiEvent(StoppedActingOnTab{mock_tab_.GetHandle()},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnMouseMove_UpdatesUiCorrectly) {
  PageTarget page_target(gfx::Point(100, 200));
  UiTabState expected_ui_tab_state{
      .agent_overlay = AgentOverlayState(
          /*is_active=*/true, /*mouse_down=*/false, page_target),
      .handoff_button = {
          .is_active = true,
          .controller = HandoffButtonState::ControlOwnership::kAgent}};
  VerifyUiEvent(MouseMove{mock_tab_.GetHandle(), page_target},
                expected_ui_tab_state);
}

TEST_F(ActorUiStateManagerUiEventUiTabScopedTest,
       OnMouseClick_UpdatesUiCorrectly) {
  UiTabState expected_ui_tab_state{
      .agent_overlay =
          AgentOverlayState(/*is_active=*/true, /*mouse_down=*/true),
      .handoff_button = {
          .is_active = true,
          .controller = HandoffButtonState::ControlOwnership::kAgent}};
  VerifyUiEvent(MouseClick{mock_tab_.GetHandle(), MouseClickType::kLeft,
                           MouseClickCount::kSingle},
                expected_ui_tab_state);
}

}  // namespace
}  // namespace actor::ui

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

using ChangeTaskState = ui::UiEventDispatcher::ChangeTaskState;
using testing::_;
using testing::Eq;
using testing::Field;
using testing::Property;
using testing::VariantWith;

class ActorTaskTest : public testing::Test {
 public:
  ActorTaskTest()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicActor, kActorFormScriptToolInterrupt}, {});
  }

  void SetUp() override {
    profile_ =
        TestingProfile::Builder()
            .AddTestingFactory(
                ActorKeyedServiceFactory::GetInstance(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<ActorKeyedServiceFake>(
                      Profile::FromBrowserContext(context));
                }))
            .Build();

    // Setup mock dispatcher for ActorTask and ExecutionEngine.
    auto mock_ui_event_dispatcher =
        std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
    mock_ui_event_dispatcher_ = mock_ui_event_dispatcher.get();

    ON_CALL(*mock_ui_event_dispatcher_, OnPreTool)
        .WillByDefault([](const ToolRequest&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });

    ON_CALL(*mock_ui_event_dispatcher_, OnPostTool)
        .WillByDefault([](const ToolRequest&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });

    ON_CALL(*mock_ui_event_dispatcher_, OnActorTaskAsyncChange)
        .WillByDefault([](const ui::UiEventDispatcher::ActorTaskAsyncChange&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });

    auto* service = ActorKeyedService::Get(profile_.get());
    TaskId task_id = service->CreateTaskForTesting(
        std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
        NoEnterprisePolicyChecker(), /*options=*/nullptr,
        mock_delegate_.GetWeakPtr());
    task_ = service->GetTask(task_id);
  }

  void ExpectStateChangeNotification(ActorTask::State new_state) {
    EXPECT_CALL(*mock_ui_event_dispatcher_,
                OnActorTaskSyncChange(VariantWith<ChangeTaskState>(
                    Field(&ChangeTaskState::new_state, Eq(new_state)))))
        .Times(1);
  }

  void ExpectTabAddedNotification(tabs::TabHandle tab_handle) {
    EXPECT_CALL(
        *mock_ui_event_dispatcher_,
        OnActorTaskAsyncChange(
            VariantWith<ui::UiEventDispatcher::AddTab>(
                Field(&ui::UiEventDispatcher::AddTab::handle, Eq(tab_handle))),
            _))
        .Times(1);
  }

  void ExpectTabRemovedNotification(tabs::TabHandle tab_handle) {
    EXPECT_CALL(
        *mock_ui_event_dispatcher_,
        OnActorTaskSyncChange(VariantWith<ui::UiEventDispatcher::RemoveTab>(
            Field(&ui::UiEventDispatcher::RemoveTab::handle, Eq(tab_handle)))))
        .Times(1);
  }

  void ExpectStopNotification(ActorTask::State final_state) {
    EXPECT_CALL(
        *mock_ui_event_dispatcher_,
        OnActorTaskSyncChange(VariantWith<ui::UiEventDispatcher::StopTask>(
            Field(&ui::UiEventDispatcher::StopTask::final_state,
                  Eq(final_state)))))
        .Times(1);
  }

  void AddTabAndVerify(tabs::MockTabInterface& tab) {
    if (!tab.GetProfile()) {
      ON_CALL(tab, GetProfile).WillByDefault(::testing::Return(profile_.get()));
    }
    ExpectTabAddedNotification(tab.GetHandle());
    AddTabToTask(tab, *task_);
    EXPECT_TRUE(task_->HasTab(tab.GetHandle()));
    EXPECT_TRUE(task_->GetTabs().contains(tab.GetHandle()));
  }

  void RemoveTabAndVerify(tabs::TabInterface& tab) {
    ExpectTabRemovedNotification(tab.GetHandle());
    task_->RemoveTab(tab.GetHandle());
    EXPECT_FALSE(task_->HasTab(tab.GetHandle()));
    EXPECT_FALSE(task_->GetTabs().contains(tab.GetHandle()));
  }

  void ExpectState(ActorTask::State state,
                   bool interrupted_needs_user_control = false) {
    EXPECT_EQ(task_->GetState(), state);
    bool is_completed = ActorTask::IsCompletedState(state);
    EXPECT_EQ(task_->IsCompleted(), is_completed);

    bool is_actor_controlled = (state == ActorTask::State::kActing ||
                                state == ActorTask::State::kReflecting ||
                                state == ActorTask::State::kWaitingOnUser ||
                                state == ActorTask::State::kCreated) &&
                               !(state == ActorTask::State::kWaitingOnUser &&
                                 interrupted_needs_user_control);
    EXPECT_EQ(task_->IsUnderActorControl(), is_actor_controlled);

    bool is_user_controlled = state == ActorTask::State::kPausedByActor ||
                              state == ActorTask::State::kPausedByUser ||
                              (state == ActorTask::State::kWaitingOnUser &&
                               interrupted_needs_user_control);
    EXPECT_EQ(task_->IsUnderUserControl(), is_user_controlled);
  }

  void TearDown() override {
    auto* service = ActorKeyedService::Get(profile_.get());
    if (task_ && !task_->IsCompleted()) {
      EXPECT_CALL(*mock_ui_event_dispatcher_,
                  OnActorTaskSyncChange(
                      VariantWith<ui::UiEventDispatcher::StopTask>(_)))
          .Times(testing::AnyNumber());
      EXPECT_CALL(*mock_ui_event_dispatcher_,
                  OnActorTaskSyncChange(
                      VariantWith<ui::UiEventDispatcher::RemoveTab>(_)))
          .Times(testing::AnyNumber());
      service->StopTask(task_->id(), ActorTask::StoppedReason::kTaskComplete);
    }
    service->ResetForTesting();
  }

  std::unique_ptr<tabs::MockTabInterface> CreateCrossProfileMockTab(
      Profile* other_profile = nullptr) {
    if (!other_profile) {
      if (!other_profile_) {
        other_profile_ = TestingProfile::Builder().Build();
      }
      other_profile = other_profile_.get();
    }
    auto mock_tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab, GetProfile)
        .WillByDefault(::testing::Return(other_profile));
    return mock_tab;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfile> other_profile_;
  MockActorTaskDelegate mock_delegate_;
  raw_ptr<ActorTask> task_;
  raw_ptr<ui::MockUiEventDispatcher> mock_ui_event_dispatcher_;
};

TEST_F(ActorTaskTest, CustomToolInterruptsWithUserControl) {
  // 1. Add a tab to the task.
  tabs::MockTabInterface mock_tab;
  AddTabAndVerify(mock_tab);

  // 2. Perform an action using the fake tool.
  std::vector<std::unique_ptr<ToolRequest>> actions;
  base::test::TestFuture<ToolCallback> on_invoke_future;
  actions.push_back(std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](ToolCallback callback) {
        task_->GetExecutionEngine().InterruptFromTool(
            /*retain_user_control=*/true);
        on_invoke_future.SetValue(std::move(callback));
      }),
      /*on_destroy=*/base::DoNothing()));

  {
    testing::InSequence s;
    // Expectation for the state change notification to kActing.
    EXPECT_CALL(
        *mock_ui_event_dispatcher_,
        OnActorTaskSyncChange(VariantWith<ChangeTaskState>(Field(
            &ChangeTaskState::new_state, Eq(ActorTask::State::kActing)))));

    // Expectation for the state change notification to kPausedByActor (due to
    // Interrupt(true)).
    EXPECT_CALL(*mock_ui_event_dispatcher_,
                OnActorTaskSyncChange(VariantWith<ChangeTaskState>(
                    Field(&ChangeTaskState::new_state,
                          Eq(ActorTask::State::kPausedByActor)))));

    // Expectation for the state change notification to kActing (resuming from
    // interrupt).
    EXPECT_CALL(
        *mock_ui_event_dispatcher_,
        OnActorTaskSyncChange(VariantWith<ChangeTaskState>(Field(
            &ChangeTaskState::new_state, Eq(ActorTask::State::kActing)))));

    // Expectation for the state change notification to kReflecting (due to
    // OnFinishedAct).
    EXPECT_CALL(
        *mock_ui_event_dispatcher_,
        OnActorTaskSyncChange(VariantWith<ChangeTaskState>(Field(
            &ChangeTaskState::new_state, Eq(ActorTask::State::kReflecting)))));
  }

  ActResultFuture future;
  task_->Act(std::move(actions), future.GetCallback());

  // Wait for the tool to be invoked.
  EXPECT_TRUE(on_invoke_future.Wait());

  // Verify the mid-invoke state.
  ExpectState(ActorTask::State::kWaitingOnUser,
              /*interrupted_needs_user_control=*/true);

  // Uninterrupt the task and let the tool run to completion.
  task_->Uninterrupt(ActorTask::State::kActing);
  std::move(on_invoke_future.Take()).Run(MakeOkResult());
  ASSERT_TRUE(future.Wait());

  EXPECT_CALL(
      *mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<ui::UiEventDispatcher::StopTask>(_)))
      .Times(testing::AnyNumber());
  EXPECT_CALL(
      *mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<ui::UiEventDispatcher::RemoveTab>(_)))
      .Times(testing::AnyNumber());

  // Stop the task.
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorTaskTest, BasicGetters) {
  EXPECT_EQ(task_->id(), TaskId(1));
  EXPECT_EQ(task_->title(), "");
  EXPECT_NE(task_->delegate(), nullptr);
  EXPECT_NE(task_->GetProfile(), nullptr);
  EXPECT_EQ(task_->get_task_duration(), ActorTask::TaskDuration::kDefault);
  EXPECT_TRUE(task_->GetEndTime().is_null());
  EXPECT_NE(task_->GetWeakPtr(), nullptr);
}

TEST_F(ActorTaskTest, StaticStateHelpers) {
  EXPECT_FALSE(ActorTask::IsCompletedState(ActorTask::State::kCreated));
  EXPECT_FALSE(ActorTask::IsCompletedState(ActorTask::State::kActing));
  EXPECT_TRUE(ActorTask::IsCompletedState(ActorTask::State::kFinished));
  EXPECT_TRUE(ActorTask::IsCompletedState(ActorTask::State::kCancelled));
  EXPECT_TRUE(ActorTask::IsCompletedState(ActorTask::State::kFailed));

  EXPECT_EQ(ActorTask::GetTaskStateFromStoppedReason(
                ActorTask::StoppedReason::kTaskComplete),
            ActorTask::State::kFinished);
  EXPECT_EQ(ActorTask::GetTaskStateFromStoppedReason(
                ActorTask::StoppedReason::kStoppedByUser),
            ActorTask::State::kCancelled);
  EXPECT_EQ(ActorTask::GetTaskStateFromStoppedReason(
                ActorTask::StoppedReason::kModelError),
            ActorTask::State::kFailed);
}

TEST_F(ActorTaskTest, StopLifecycle) {
  ExpectState(ActorTask::State::kCreated);
  ExpectStopNotification(ActorTask::State::kFinished);
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
  ExpectState(ActorTask::State::kFinished);
  EXPECT_FALSE(task_->GetEndTime().is_null());
}

TEST_F(ActorTaskTest, PauseAndResume) {
  ExpectState(ActorTask::State::kCreated);

  // Pause from actor.
  ExpectStateChangeNotification(ActorTask::State::kPausedByActor);
  task_->Pause(/*from_actor=*/true);
  ExpectState(ActorTask::State::kPausedByActor);

  // Resume.
  ExpectStateChangeNotification(ActorTask::State::kReflecting);
  task_->Resume();
  ExpectState(ActorTask::State::kReflecting);

  // Pause from user.
  ExpectStateChangeNotification(ActorTask::State::kPausedByUser);
  task_->Pause(/*from_actor=*/false);
  ExpectState(ActorTask::State::kPausedByUser);
}

TEST_F(ActorTaskTest, TabManagement) {
  tabs::MockTabInterface mock_tab;

  // Initial state has no tabs.
  EXPECT_TRUE(task_->GetTabs().empty());
  EXPECT_FALSE(task_->HasTab(mock_tab.GetHandle()));

  // Add and verify.
  AddTabAndVerify(mock_tab);

  // Remove and verify.
  RemoveTabAndVerify(mock_tab);
}

TEST_F(ActorTaskTest, ObserveTabOnceAndActing) {
  tabs::MockTabInterface mock_tab;

  // Before adding, it's not acting.
  EXPECT_FALSE(task_->IsActingOnTab(mock_tab.GetHandle()));

  // Add tab.
  AddTabAndVerify(mock_tab);

  // Under actor control, it is acting on the tab.
  EXPECT_TRUE(task_->IsActingOnTab(mock_tab.GetHandle()));

  // Transitively verify ObserveTabOnce does not double add if already present.
  task_->ObserveTabOnce(mock_tab.GetHandle());
  EXPECT_TRUE(task_->GetLastActedTabs().contains(mock_tab.GetHandle()));
}

TEST_F(ActorTaskTest, GetLastActuatedTabPreservedAfterTaskCompleted) {
  tabs::MockTabInterface mock_tab;

  AddTabAndVerify(mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());

  // Stop task. controlled_tabs_ should be cleared, but GetLastActuatedTab()
  // should still return mock_tab.
  ExpectStopNotification(ActorTask::State::kFinished);
  ExpectTabRemovedNotification(mock_tab.GetHandle());
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);

  EXPECT_TRUE(task_->GetTabs().empty());
  EXPECT_TRUE(task_->GetLastActedTabs().empty());
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());
}

TEST_F(ActorTaskTest, GetLastActuatedTabFiltersDestroyedTabsAfterStop) {
  auto mock_tab = std::make_unique<tabs::MockTabInterface>();
  tabs::TabHandle handle = mock_tab->GetHandle();

  AddTabAndVerify(*mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTab(), mock_tab.get());
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(), handle.raw_value());

  ExpectStopNotification(ActorTask::State::kFinished);
  ExpectTabRemovedNotification(handle);
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);

  EXPECT_EQ(task_->GetLastActuatedTab(), mock_tab.get());
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(), handle.raw_value());

  // Destroy the tab.
  mock_tab.reset();

  // After tab destruction, handle.Get() is null, so it shouldn't be returned.
  EXPECT_EQ(task_->GetLastActuatedTab(), nullptr);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().Get(), nullptr);
}

TEST_F(ActorTaskTest, GetLastActuatedTabWhileActingAndPaused) {
  tabs::MockTabInterface mock_tab;

  // Add tab to task.
  AddTabAndVerify(mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());

  // Transition to Acting state.
  ExpectStateChangeNotification(ActorTask::State::kActing);
  task_->SetState(ActorTask::State::kActing);
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());

  // Pause task from actor.
  ExpectStateChangeNotification(ActorTask::State::kPausedByActor);
  task_->Pause(/*from_actor=*/true);
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());

  // Resume task.
  ExpectStateChangeNotification(ActorTask::State::kReflecting);
  task_->Resume();
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());

  // Transition back to Acting state.
  ExpectStateChangeNotification(ActorTask::State::kActing);
  task_->SetState(ActorTask::State::kActing);
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());

  // Pause task from user.
  ExpectStateChangeNotification(ActorTask::State::kPausedByUser);
  task_->Pause(/*from_actor=*/false);
  EXPECT_EQ(task_->GetLastActuatedTab(), &mock_tab);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            mock_tab.GetHandle().raw_value());

  // Stop task to clean up controlled tabs before mock_tab is destroyed.
  ExpectStopNotification(ActorTask::State::kFinished);
  ExpectTabRemovedNotification(mock_tab.GetHandle());
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorTaskTest, GetLastActuatedTabSwitchesBetweenTabs) {
  tabs::MockTabInterface tab_a;
  tabs::MockTabInterface tab_b;

  // 1. Actuate on Tab A.
  AddTabAndVerify(tab_a);
  EXPECT_EQ(task_->GetLastActuatedTab(), &tab_a);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            tab_a.GetHandle().raw_value());

  // 2. Actuate on Tab B.
  AddTabAndVerify(tab_b);
  EXPECT_EQ(task_->GetLastActuatedTab(), &tab_b);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            tab_b.GetHandle().raw_value());

  // 3. Actuate on Tab A again (which is already controlled).
  AddTabToTask(tab_a, *task_);
  EXPECT_EQ(task_->GetLastActuatedTab(), &tab_a);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            tab_a.GetHandle().raw_value());

  // 4. Stop task; the last actuated tab (Tab A) is preserved.
  ExpectStopNotification(ActorTask::State::kFinished);
  ExpectTabRemovedNotification(tab_a.GetHandle());
  ExpectTabRemovedNotification(tab_b.GetHandle());
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);

  EXPECT_TRUE(task_->GetTabs().empty());
  EXPECT_EQ(task_->GetLastActuatedTab(), &tab_a);
  EXPECT_EQ(task_->GetLastActuatedTabHandle().raw_value(),
            tab_a.GetHandle().raw_value());
}

TEST_F(ActorTaskTest, AdditionalTabObservations) {
  std::vector<optimization_guide::proto::TabObservation> observations;
  optimization_guide::proto::TabObservation obs;
  obs.set_id(123);
  observations.push_back(obs);

  // Enable transient feature to add additional observations.
  base::test::ScopedFeatureList feature_list(
      kGlicActorLoadAndExtractContentTool);
  task_->AddAdditionalTabObservations(observations);

  const auto& result = task_->GetAdditionalTabObservations();
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].id(), 123);
}

TEST_F(ActorTaskTest, CancelOngoingActions) {
  // Perform an action using FakeToolRequest.
  std::vector<std::unique_ptr<ToolRequest>> actions;
  base::test::TestFuture<ToolCallback> on_invoke_future;
  actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));

  // UI Event Dispatcher should expect the transitions
  ExpectStateChangeNotification(ActorTask::State::kActing);

  // Act.
  ActResultFuture future;
  task_->Act(std::move(actions), future.GetCallback());

  // Wait for invoke.
  EXPECT_TRUE(on_invoke_future.Wait());

  // State changes to kReflecting when cancelled.
  ExpectStateChangeNotification(ActorTask::State::kReflecting);

  // Cancel actions.
  EXPECT_TRUE(
      task_->CancelOngoingActions(mojom::ActionResultCode::kTaskPaused));

  // Verify the callback was invoked with task paused result.
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get()[0].result->code, mojom::ActionResultCode::kTaskPaused);
}

TEST_F(ActorTaskTest, ActEdgeCases) {
  // Case 1: Act when paused.
  ExpectStateChangeNotification(ActorTask::State::kPausedByActor);
  task_->Pause(/*from_actor=*/true);
  ExpectState(ActorTask::State::kPausedByActor);

  std::vector<std::unique_ptr<ToolRequest>> actions;
  ActResultFuture future_paused;
  task_->Act(std::move(actions), future_paused.GetCallback());
  ASSERT_TRUE(future_paused.Wait());
  EXPECT_EQ(future_paused.Get()[0].result->code,
            mojom::ActionResultCode::kTaskPaused);

  // Resume so we can stop it.
  ExpectStateChangeNotification(ActorTask::State::kReflecting);
  task_->Resume();
  ExpectStopNotification(ActorTask::State::kFinished);
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
  ExpectState(ActorTask::State::kFinished);

  // Case 2: Act when completed.
  std::vector<std::unique_ptr<ToolRequest>> actions_completed;
  ActResultFuture future_completed;
  task_->Act(std::move(actions_completed), future_completed.GetCallback());
  ASSERT_TRUE(future_completed.Wait());
  EXPECT_EQ(future_completed.Get()[0].result->code,
            mojom::ActionResultCode::kTaskWentAway);
}

TEST_F(ActorTaskTest, PauseEdgeCases) {
  // Pause when already completed has no effect.
  ExpectStopNotification(ActorTask::State::kFinished);
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
  ExpectState(ActorTask::State::kFinished);

  task_->Pause(/*from_actor=*/true);
  ExpectState(ActorTask::State::kFinished);
}

TEST_F(ActorTaskTest, ResumeEdgeCases) {
  // Resume when not paused has no effect.
  ExpectState(ActorTask::State::kCreated);
  task_->Resume();
  ExpectState(ActorTask::State::kCreated);
}

TEST_F(ActorTaskTest, InterruptEdgeCases) {
  // Interrupt when not active (e.g., Created is not Acting/Reflecting) has no
  // effect.
  ExpectState(ActorTask::State::kCreated);
  task_->Interrupt();
  ExpectState(ActorTask::State::kCreated);

  // Uninterrupt when not waiting has no effect.
  task_->Uninterrupt(ActorTask::State::kActing);
  ExpectState(ActorTask::State::kCreated);
}

TEST_F(ActorTaskTest, MultipleActions) {
  // 1. Perform multiple actions using FakeToolRequest.
  std::vector<std::unique_ptr<ToolRequest>> actions;
  base::test::TestFuture<ToolCallback> on_invoke_future_1;
  actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future_1.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));

  base::test::TestFuture<ToolCallback> on_invoke_future_2;
  actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future_2.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));

  // UI Event Dispatcher expects to transition to kActing.
  ExpectStateChangeNotification(ActorTask::State::kActing);

  // Act.
  ActResultFuture future;
  task_->Act(std::move(actions), future.GetCallback());

  // Wait for first tool to invoke.
  EXPECT_TRUE(on_invoke_future_1.Wait());
  std::move(on_invoke_future_1.Take()).Run(MakeOkResult());

  // Wait for second tool to invoke.
  EXPECT_TRUE(on_invoke_future_2.Wait());
  std::move(on_invoke_future_2.Take()).Run(MakeOkResult());

  // Once all actions finish, the task transitions to kReflecting.
  ExpectStateChangeNotification(ActorTask::State::kReflecting);

  // Verify the callback was invoked and both actions succeeded.
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().size(), 2u);
  EXPECT_EQ(future.Get()[0].result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(future.Get()[1].result->code, mojom::ActionResultCode::kOk);
}

class ActorTaskCompletionMetricsTest : public testing::Test {
 public:
  ActorTaskCompletionMetricsTest()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
  }

  void SetUp() override {
    profile_ =
        TestingProfile::Builder()
            .AddTestingFactory(
                ActorKeyedServiceFactory::GetInstance(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<ActorKeyedServiceFake>(
                      Profile::FromBrowserContext(context));
                }))
            .Build();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  MockActorTaskDelegate mock_delegate_;
};

TEST_F(ActorTaskCompletionMetricsTest,
       InvocationSourceCompletionMetrics_UniversalCart) {
  base::HistogramTester histograms;

  auto task_options = actor::webui::mojom::TaskOptions::New();
  task_options->title = "Completion Metrics Task";

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(task_options),
      mock_delegate_.GetWeakPtr(),
      glic::mojom::InvocationSource::kUniversalCart);
  ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->initial_invocation_source(),
            glic::mojom::InvocationSource::kUniversalCart);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kActorRecordInvocationSourceCompletionMetrics);

  task->Stop(ActorTask::StoppedReason::kTaskComplete);

  histograms.ExpectTotalCount(
      "Actor.Task.Duration.WallClock.Completed.UniversalCart", 1);
  histograms.ExpectTotalCount("Actor.Task.Duration.Completed.UniversalCart", 1);
  histograms.ExpectTotalCount("Actor.Task.Count.Completed.UniversalCart", 1);
}

TEST_F(ActorTaskCompletionMetricsTest,
       InvocationSourceCompletionMetrics_Other) {
  base::HistogramTester histograms;

  auto task_options = actor::webui::mojom::TaskOptions::New();
  task_options->title = "Completion Metrics Task";

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(task_options),
      mock_delegate_.GetWeakPtr(), glic::mojom::InvocationSource::kOsButton);
  ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->initial_invocation_source(),
            glic::mojom::InvocationSource::kOsButton);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kActorRecordInvocationSourceCompletionMetrics);

  task->Stop(ActorTask::StoppedReason::kTaskComplete);

  histograms.ExpectTotalCount("Actor.Task.Duration.WallClock.Completed.Other",
                              1);
  histograms.ExpectTotalCount("Actor.Task.Duration.Completed.Other", 1);
  histograms.ExpectTotalCount("Actor.Task.Count.Completed.Other", 1);
}

TEST_F(ActorTaskCompletionMetricsTest,
       InvocationSourceCompletionMetrics_Disabled) {
  base::HistogramTester histograms;

  auto task_options = actor::webui::mojom::TaskOptions::New();
  task_options->title = "Completion Metrics Task";

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(task_options),
      mock_delegate_.GetWeakPtr(),
      glic::mojom::InvocationSource::kUniversalCart);
  ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(task, nullptr);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kActorRecordInvocationSourceCompletionMetrics);

  task->Stop(ActorTask::StoppedReason::kTaskComplete);

  histograms.ExpectTotalCount(
      "Actor.Task.Duration.WallClock.Completed.UniversalCart", 0);
  histograms.ExpectTotalCount("Actor.Task.Duration.Completed.UniversalCart", 0);
  histograms.ExpectTotalCount("Actor.Task.Count.Completed.UniversalCart", 0);
}

TEST_F(ActorTaskCompletionMetricsTest,
       InvocationSourceCompletionMetrics_DefaultInvocationSource) {
  base::HistogramTester histograms;

  auto task_options = actor::webui::mojom::TaskOptions::New();
  task_options->title = "Completion Metrics Task";

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(task_options),
      mock_delegate_.GetWeakPtr());
  ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->initial_invocation_source(), std::nullopt);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kActorRecordInvocationSourceCompletionMetrics);

  task->Stop(ActorTask::StoppedReason::kTaskComplete);

  // Do not record any histograms if invocation source is not set.
  histograms.ExpectTotalCount("Actor.Task.Duration.WallClock.Completed.Other",
                              0);
  histograms.ExpectTotalCount("Actor.Task.Duration.Completed.Other", 0);
  histograms.ExpectTotalCount("Actor.Task.Count.Completed.Other", 0);
}

TEST_F(ActorTaskTest, AddTab_RejectsNonExistentTab) {
  tabs::TabHandle non_existent_handle(99999);
  base::test::TestFuture<mojom::ActionResultPtr> add_tab_future;
  task_->AddTab(non_existent_handle, /*stop_task_on_detach=*/true,
                add_tab_future.GetCallback());
  auto result = add_tab_future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kTabWentAway);
  EXPECT_FALSE(task_->HasTab(non_existent_handle));
  EXPECT_FALSE(task_->GetTabs().contains(non_existent_handle));
}

TEST_F(ActorTaskTest, AddTab_RejectsCrossProfileTab) {
  std::unique_ptr<tabs::MockTabInterface> cross_profile_tab =
      CreateCrossProfileMockTab();

  base::test::TestFuture<mojom::ActionResultPtr> add_tab_future;
  task_->AddTab(cross_profile_tab->GetHandle(), /*stop_task_on_detach=*/true,
                add_tab_future.GetCallback());
  auto result = add_tab_future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kActionTargetCrossProfile);
  EXPECT_FALSE(task_->HasTab(cross_profile_tab->GetHandle()));
  EXPECT_FALSE(task_->GetTabs().contains(cross_profile_tab->GetHandle()));
}

TEST_F(ActorTaskTest, AddTab_RejectsCrossProfileTabEvenWithNullContents) {
  std::unique_ptr<tabs::MockTabInterface> cross_profile_tab =
      CreateCrossProfileMockTab();
  ON_CALL(*cross_profile_tab, GetContents)
      .WillByDefault(::testing::Return(nullptr));

  base::test::TestFuture<mojom::ActionResultPtr> add_tab_future;
  task_->AddTab(cross_profile_tab->GetHandle(), /*stop_task_on_detach=*/true,
                add_tab_future.GetCallback());
  auto result = add_tab_future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kActionTargetCrossProfile);
  EXPECT_FALSE(task_->HasTab(cross_profile_tab->GetHandle()));
  EXPECT_FALSE(task_->GetTabs().contains(cross_profile_tab->GetHandle()));
}

TEST_F(ActorTaskTest, ObserveTabOnce_RejectsNonExistentAndCrossProfileTab) {
  tabs::TabHandle non_existent_handle(99999);
  task_->ObserveTabOnce(non_existent_handle);
  EXPECT_FALSE(task_->GetLastActedTabs().contains(non_existent_handle));

  std::unique_ptr<tabs::MockTabInterface> cross_profile_tab =
      CreateCrossProfileMockTab();

  task_->ObserveTabOnce(cross_profile_tab->GetHandle());
  EXPECT_FALSE(
      task_->GetLastActedTabs().contains(cross_profile_tab->GetHandle()));
}

}  // namespace

}  // namespace actor

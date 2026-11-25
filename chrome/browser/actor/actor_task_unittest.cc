// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_surface_tab_helper.h"
#include "chrome/browser/actor/actor_task_options.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_ui_mode.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace actor {

namespace {

using ChangeTaskState = ui::UiEventDispatcher::ChangeTaskState;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Property;
using testing::VariantWith;

class ActorTaskTest : public testing::Test {
 public:
  ActorTaskTest()
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

    // Setup mock dispatcher for ActorTask and ExecutionEngine.
    auto mock_ui_event_dispatcher =
        std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
    mock_ui_event_dispatcher_ = mock_ui_event_dispatcher.get();
    ConfigureMockDispatcher(*mock_ui_event_dispatcher_);

    auto* service = ActorKeyedService::Get(profile_.get());
    ActorTaskOptions options;
    options.delegate = mock_delegate_.GetWeakPtr();
    TaskId task_id = service->CreateTaskForTesting(
        std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
        NoEnterprisePolicyChecker(), std::move(options));
    task_ = service->GetTask(task_id);
  }

  void ConfigureMockDispatcher(ui::MockUiEventDispatcher& dispatcher) {
    ON_CALL(dispatcher, OnPreTool)
        .WillByDefault([](const ToolRequest&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });
    ON_CALL(dispatcher, OnPostTool)
        .WillByDefault([](const ToolRequest&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });
    ON_CALL(dispatcher, OnActorTaskAsyncChange)
        .WillByDefault([](const ui::UiEventDispatcher::ActorTaskAsyncChange&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });
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
    registry().OnTabCreated(tab);
    ExpectTabAddedNotification(tab.GetHandle());
    AddTabToTask(tab, *task_);
    EXPECT_TRUE(task_->HasTab(tab.GetHandle()));
    EXPECT_TRUE(task_->GetTabs().contains(tab.GetHandle()));
  }

  ActorSurfaceRegistry& registry() {
    return ActorKeyedService::Get(profile_.get())->GetSurfaceRegistry();
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
    }
    for (TaskId task_id :
         service->FindTaskIdsInActive([](const ActorTask&) { return true; })) {
      service->StopTask(task_id, ActorTask::StoppedReason::kTaskComplete);
    }
    service->ResetForTesting();
  }

  ActorTask* CreateAdditionalTask(
      ui::MockUiEventDispatcher** out_dispatcher = nullptr,
      ActorTaskOptions options = {}) {
    auto dispatcher =
        std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
    ConfigureMockDispatcher(*dispatcher);
    if (out_dispatcher) {
      *out_dispatcher = dispatcher.get();
    }
    auto* service = ActorKeyedService::Get(profile_.get());
    options.delegate = mock_delegate_.GetWeakPtr();
    TaskId task_id = service->CreateTaskForTesting(
        std::move(dispatcher), TestTaskSourceInfo(),
        NoEnterprisePolicyChecker(), std::move(options));
    return service->GetTask(task_id);
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    content::WebContents::CreateParams params(profile_.get());
    params.enable_wake_locks = false;
    return base::WrapUnique(
        content::WebContentsTester::CreateTestWebContents(params));
  }

  mojom::ActionResultCode AddTabAndGetCode(ActorTask& task,
                                           tabs::TabHandle tab_handle,
                                           bool stop_task_on_detach = true) {
    base::test::TestFuture<mojom::ActionResultPtr> future;
    task.AddTab(tab_handle, stop_task_on_detach, future.GetCallback());
    return future.Take()->code;
  }

  mojom::ActionResultCode ActOnTabAndGetCode(ActorTask& task,
                                             tabs::TabHandle tab_handle,
                                             bool* out_invoked = nullptr) {
    std::vector<std::unique_ptr<ToolRequest>> actions;
    actions.push_back(std::make_unique<FakeToolRequest>(
        base::BindLambdaForTesting(
            [out_invoked](DeprecatedToolCallback callback) {
              if (out_invoked) {
                *out_invoked = true;
              }
              std::move(callback).Run(MakeOkResult());
            }),
        /*on_destroy=*/base::DoNothing(),
        ActorSurfaceHandle::From(tab_handle)));
    ActResultFuture future;
    task.Act(std::move(actions), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get()[0].tool_result.deprecated_action_result()->code;
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
  content::RenderViewHostTestEnabler rvh_test_enabler_;
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
  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  actions.push_back(std::make_unique<FakeToolRequest>(
      base::BindLambdaForTesting([&](DeprecatedToolCallback callback) {
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

TEST_F(ActorTaskTest, SetSourceIdBackfillsSourceInfo) {
  EXPECT_FALSE(task_->source_info().id.has_value());

  task_->SetSourceId("initial-id");
  EXPECT_EQ(task_->source_info().id, "initial-id");

  // Idempotent backfill with the same ID succeeds.
  task_->SetSourceId("initial-id");
  EXPECT_EQ(task_->source_info().id, "initial-id");
}

TEST_F(ActorTaskTest, SetStepProgressSendsEveryUpdateToUi) {
  using UpdateTaskStepProgress = ui::UiEventDispatcher::UpdateTaskStepProgress;
  EXPECT_CALL(
      *mock_ui_event_dispatcher_,
      OnActorTaskSyncChange(VariantWith<UpdateTaskStepProgress>(AllOf(
          Field(&UpdateTaskStepProgress::task_id, Eq(task_->id())),
          Field(&UpdateTaskStepProgress::step_progress, Eq("Searching"))))))
      .Times(2);
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnActorTaskSyncChange(VariantWith<UpdateTaskStepProgress>(AllOf(
                  Field(&UpdateTaskStepProgress::task_id, Eq(task_->id())),
                  Field(&UpdateTaskStepProgress::step_progress,
                        Eq("Adding to cart"))))))
      .Times(1);
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnActorTaskSyncChange(VariantWith<UpdateTaskStepProgress>(AllOf(
                  Field(&UpdateTaskStepProgress::task_id, Eq(task_->id())),
                  Field(&UpdateTaskStepProgress::step_progress, Eq(""))))))
      .Times(1);

  task_->SetStepProgress("Searching");
  task_->SetStepProgress("Searching");
  task_->SetStepProgress("Adding to cart");
  task_->SetStepProgress("");
}

TEST_F(ActorTaskTest, OnActRequestedSendsSyncChangeToUi) {
  using ActRequested = ui::UiEventDispatcher::ActRequested;
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnActorTaskSyncChange(VariantWith<ActRequested>(
                  Field(&ActRequested::task_id, Eq(task_->id())))))
      .Times(1);
  task_->OnActRequested();
}

TEST_F(ActorTaskTest, SetStepProgressUpdatesStateBeforeUiEvent) {
  using UpdateTaskStepProgress = ui::UiEventDispatcher::UpdateTaskStepProgress;
  EXPECT_CALL(*mock_ui_event_dispatcher_,
              OnActorTaskSyncChange(VariantWith<UpdateTaskStepProgress>(_)))
      .WillOnce([&] { EXPECT_EQ(task_->step_progress(), "Searching"); });

  task_->SetStepProgress("Searching");
}

TEST_F(ActorTaskTest, SetStepProgressNotifiesObserversOnlyOnChange) {
  std::vector<std::string> updates;
  base::CallbackListSubscription subscription =
      ActorKeyedService::Get(profile_.get())
          ->AddTaskStepProgressChangedCallback(base::BindLambdaForTesting(
              [&](ActorTask&, const std::string& step_progress) {
                updates.push_back(step_progress);
              }));

  task_->SetStepProgress("Searching");
  task_->SetStepProgress("Searching");
  task_->SetStepProgress("Adding to cart");

  EXPECT_THAT(updates, ElementsAre("Searching", "Adding to cart"));
  EXPECT_EQ(task_->step_progress(), "Adding to cart");
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
  EXPECT_TRUE(task_->GetLastActedActorSurfaces().contains(
      ActorSurfaceHandle::From(mock_tab.GetHandle())));
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
  EXPECT_TRUE(task_->GetLastActedActorSurfaces().empty());
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
  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
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
  EXPECT_EQ(future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskPaused);
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
  EXPECT_EQ(future_paused.Get()[0].tool_result.deprecated_action_result()->code,
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
  EXPECT_EQ(
      future_completed.Get()[0].tool_result.deprecated_action_result()->code,
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
  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future_1;
  actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future_1.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));

  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future_2;
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
  EXPECT_EQ(future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
  EXPECT_EQ(future.Get()[1].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
}

// Returns a single action that is never expected to start.
std::vector<std::unique_ptr<ToolRequest>> MakeActionsNotExpectedToStart(
    ActorSurfaceHandle actor_surface_handle) {
  std::vector<std::unique_ptr<ToolRequest>> actions;
  actions.push_back(std::make_unique<FakeToolRequest>(
      base::BindOnce(
          [](DeprecatedToolCallback) { ADD_FAILURE() << "Tool invoked"; }),
      /*on_destroy=*/base::DoNothing(), actor_surface_handle));
  return actions;
}

// `on_actions_accepted` runs once the task has entered `kActing` for the
// actions, before the first of them is invoked.
TEST_F(ActorTaskTest, OnActionsAcceptedRunsBeforeFirstTool) {
  std::vector<std::unique_ptr<ToolRequest>> actions;
  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));

  bool accepted = false;
  ActResultFuture future;
  task_->Act(std::move(actions), base::BindLambdaForTesting([&] {
               EXPECT_EQ(task_->GetState(), ActorTask::State::kActing);
               EXPECT_FALSE(on_invoke_future.IsReady());
               accepted = true;
             }),
             future.GetCallback());

  ASSERT_TRUE(on_invoke_future.Wait());
  EXPECT_TRUE(accepted);
  std::move(on_invoke_future.Take()).Run(MakeOkResult());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
}

// If `on_actions_accepted` stops the task, the actions don't start and the
// call reports that the task went away.
TEST_F(ActorTaskTest, OnActionsAcceptedCanStopTask) {
  ActResultFuture future;
  task_->Act(MakeActionsNotExpectedToStart(ActorSurfaceHandle::Null()),
             base::BindLambdaForTesting(
                 [&] { task_->Stop(ActorTask::StoppedReason::kTaskComplete); }),
             future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskWentAway);
  // Run pending tasks, in case the actions start asynchronously. This also
  // deletes the stopped task, along with its UI event dispatcher.
  task_ = nullptr;
  mock_ui_event_dispatcher_ = nullptr;
  task_environment_.FastForwardBy(base::Seconds(1));
}

// If `on_actions_accepted` pauses the task, the actions don't start and the
// call reports that the task was paused.
TEST_F(ActorTaskTest, OnActionsAcceptedCanPauseTask) {
  ActResultFuture future;
  task_->Act(
      MakeActionsNotExpectedToStart(ActorSurfaceHandle::Null()),
      base::BindLambdaForTesting([&] { task_->Pause(/*from_actor=*/true); }),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskPaused);
  // Run pending tasks, in case the actions start asynchronously.
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectState(ActorTask::State::kPausedByActor);
}

TEST_F(ActorTaskTest, OnActionsAcceptedNotRunWhenPausedOrStopped) {
  task_->Pause(/*from_actor=*/true);
  ActResultFuture paused_future;
  task_->Act(MakeActionsNotExpectedToStart(ActorSurfaceHandle::Null()),
             base::MakeExpectedNotRunClosure(FROM_HERE),
             paused_future.GetCallback());
  ASSERT_TRUE(paused_future.Wait());
  EXPECT_EQ(paused_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskPaused);

  task_->Resume();
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
  ActResultFuture stopped_future;
  task_->Act(MakeActionsNotExpectedToStart(ActorSurfaceHandle::Null()),
             base::MakeExpectedNotRunClosure(FROM_HERE),
             stopped_future.GetCallback());
  ASSERT_TRUE(stopped_future.Wait());
  EXPECT_EQ(
      stopped_future.Get()[0].tool_result.deprecated_action_result()->code,
      mojom::ActionResultCode::kTaskWentAway);
}

TEST_F(ActorTaskTest, OnActionsAcceptedNotRunWhenAddingTabFails) {
  ActResultFuture future;
  // The handle resolves to no surface, so adding it fails.
  task_->Act(
      MakeActionsNotExpectedToStart(ActorSurfaceHandle::CreateForTesting()),
      base::MakeExpectedNotRunClosure(FROM_HERE), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTabWentAway);
}

// The execution engine rejects actions while it is still executing earlier
// ones.
TEST_F(ActorTaskTest, OnActionsAcceptedNotRunWhileOtherActionsInProgress) {
  std::vector<std::unique_ptr<ToolRequest>> first_actions;
  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  first_actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));
  base::test::TestFuture<void> first_accepted;
  ActResultFuture first_future;
  task_->Act(std::move(first_actions), first_accepted.GetCallback(),
             first_future.GetCallback());
  ASSERT_TRUE(on_invoke_future.Wait());
  EXPECT_TRUE(first_accepted.IsReady());

  ActResultFuture second_future;
  task_->Act(MakeActionsNotExpectedToStart(ActorSurfaceHandle::Null()),
             base::MakeExpectedNotRunClosure(FROM_HERE),
             second_future.GetCallback());
  ASSERT_TRUE(second_future.Wait());
  EXPECT_EQ(second_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kExecutionEngineExistingAction);
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

  ActorTaskOptions options;
  options.webui_task_options = actor::webui::mojom::TaskOptions::New();
  options.webui_task_options->title = "Completion Metrics Task";
  options.delegate = mock_delegate_.GetWeakPtr();
  options.glic_initial_invocation_source =
      glic::mojom::InvocationSource::kUniversalCart;

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(options));
  ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->glic_initial_invocation_source(),
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

  ActorTaskOptions options;
  options.webui_task_options = actor::webui::mojom::TaskOptions::New();
  options.webui_task_options->title = "Completion Metrics Task";
  options.delegate = mock_delegate_.GetWeakPtr();
  options.glic_initial_invocation_source =
      glic::mojom::InvocationSource::kOsButton;

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(options));
  ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->glic_initial_invocation_source(),
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

  ActorTaskOptions options;
  options.webui_task_options = actor::webui::mojom::TaskOptions::New();
  options.webui_task_options->title = "Completion Metrics Task";
  options.delegate = mock_delegate_.GetWeakPtr();
  options.glic_initial_invocation_source =
      glic::mojom::InvocationSource::kUniversalCart;

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(options));
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

  ActorTaskOptions options;
  options.webui_task_options = actor::webui::mojom::TaskOptions::New();
  options.webui_task_options->title = "Completion Metrics Task";
  options.delegate = mock_delegate_.GetWeakPtr();

  auto* service = ActorKeyedService::Get(profile_.get());
  auto mock_ui_event_dispatcher =
      std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
  TaskId task_id = service->CreateTaskForTesting(
      std::move(mock_ui_event_dispatcher), TestTaskSourceInfo(),
      NoEnterprisePolicyChecker(), std::move(options));
  ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(task, nullptr);
  EXPECT_EQ(task->glic_initial_invocation_source(), std::nullopt);

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
  // The tab has no surface in this task's profile, so it can't be added and is
  // reported as gone.
  EXPECT_EQ(result->code, mojom::ActionResultCode::kTabWentAway);
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
  // The tab has no surface in this task's profile, so it can't be added and is
  // reported as gone.
  EXPECT_EQ(result->code, mojom::ActionResultCode::kTabWentAway);
  EXPECT_FALSE(task_->HasTab(cross_profile_tab->GetHandle()));
  EXPECT_FALSE(task_->GetTabs().contains(cross_profile_tab->GetHandle()));
}

TEST_F(ActorTaskTest, AddActorSurface_RejectsNonExistentSurface) {
  // A handle whose surface has been destroyed resolves to nothing.
  ::ui::UnownedUserDataHost user_data_host;
  tabs::MockTabInterface tab;
  ON_CALL(tab, GetProfile).WillByDefault(::testing::Return(profile_.get()));
  ON_CALL(tab, GetUnownedUserDataHost())
      .WillByDefault(::testing::ReturnRef(user_data_host));
  auto actor_surface_tab_helper = std::make_unique<ActorSurfaceTabHelper>(tab);
  const ActorSurfaceHandle stale_handle =
      ActorSurfaceHandle::From(tab.GetHandle());
  ASSERT_FALSE(stale_handle.is_null());
  actor_surface_tab_helper.reset();
  // Let the destroyed surface's tab mapping expire (it is kept until the end of
  // the current task).
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(stale_handle.Get());

  base::test::TestFuture<mojom::ActionResultPtr> add_actor_surface_future;
  task_->AddActorSurface(stale_handle,
                         /*stop_task_on_detach=*/true,
                         add_actor_surface_future.GetCallback());
  auto result = add_actor_surface_future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kTabWentAway);
}

TEST_F(ActorTaskTest, AddActorSurface_RejectsCrossProfileSurface) {
  std::unique_ptr<tabs::MockTabInterface> cross_profile_tab =
      CreateCrossProfileMockTab();
  ::ui::UnownedUserDataHost user_data_host;
  ON_CALL(*cross_profile_tab, GetUnownedUserDataHost())
      .WillByDefault(::testing::ReturnRef(user_data_host));
  ActorSurfaceTabHelper actor_surface_tab_helper(*cross_profile_tab);
  const ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::From(cross_profile_tab->GetHandle());
  ASSERT_FALSE(actor_surface_handle.is_null());

  // The surface exists but isn't in this profile's ActorSurfaceRegistry.
  base::test::TestFuture<mojom::ActionResultPtr> add_actor_surface_future;
  task_->AddActorSurface(actor_surface_handle,
                         /*stop_task_on_detach=*/true,
                         add_actor_surface_future.GetCallback());
  auto result = add_actor_surface_future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kActionTargetCrossProfile);
  EXPECT_FALSE(task_->HasTab(cross_profile_tab->GetHandle()));
}

TEST_F(ActorTaskTest, ObserveTabOnce_RejectsNonExistentAndCrossProfileTab) {
  tabs::TabHandle non_existent_handle(99999);
  task_->ObserveTabOnce(non_existent_handle);
  EXPECT_FALSE(task_->GetLastActedActorSurfaces().contains(
      ActorSurfaceHandle::From(non_existent_handle)));

  std::unique_ptr<tabs::MockTabInterface> cross_profile_tab =
      CreateCrossProfileMockTab();

  task_->ObserveTabOnce(cross_profile_tab->GetHandle());
  EXPECT_FALSE(task_->GetLastActedActorSurfaces().contains(
      ActorSurfaceHandle::From(cross_profile_tab->GetHandle())));
}

// Uses a mock tab that has a real WebContents and a registered surface, as
// real tabs do.
class ActorTaskSurfaceTest : public ActorTaskTest {
 public:
  void SetUp() override {
    ActorTaskTest::SetUp();
    web_contents_ = CreateTestWebContents();
    ON_CALL(mock_tab_, GetContents)
        .WillByDefault(::testing::Return(web_contents_.get()));
    ON_CALL(mock_tab_, GetProfile)
        .WillByDefault(::testing::Return(profile_.get()));
    registry().OnTabCreated(mock_tab_);
  }

 protected:
  ActorKeyedService& service() {
    return *ActorKeyedService::Get(profile_.get());
  }

  std::unique_ptr<content::WebContents> web_contents_;
  tabs::MockTabInterface mock_tab_;
};

TEST_F(ActorTaskSurfaceTest, ControlsPageThroughSurface) {
  AddTabAndVerify(mock_tab_);
  EXPECT_TRUE(web_contents_->IsBeingCaptured());

  RemoveTabAndVerify(mock_tab_);
  EXPECT_FALSE(web_contents_->IsBeingCaptured());
}

TEST_F(ActorTaskSurfaceTest, SurfaceApis) {
  const ActorSurfaceHandle actor_surface_handle =
      ActorSurfaceHandle::From(mock_tab_.GetHandle());
  ASSERT_FALSE(actor_surface_handle.is_null());

  ExpectTabAddedNotification(mock_tab_.GetHandle());
  base::test::TestFuture<mojom::ActionResultPtr> add_future;
  task_->AddActorSurface(actor_surface_handle, /*stop_task_on_detach=*/true,
                         add_future.GetCallback());
  ExpectOkResult(add_future);
  EXPECT_TRUE(task_->HasActorSurface(actor_surface_handle));
  EXPECT_TRUE(task_->IsActingOnActorSurface(actor_surface_handle));
  EXPECT_THAT(task_->GetActorSurfaces(),
              ::testing::UnorderedElementsAre(actor_surface_handle));
  EXPECT_THAT(task_->GetLastActedActorSurfaces(),
              ::testing::UnorderedElementsAre(actor_surface_handle));
  EXPECT_EQ(task_->GetLastActuatedActorSurface(), actor_surface_handle);

  ExpectTabRemovedNotification(mock_tab_.GetHandle());
  task_->RemoveActorSurface(actor_surface_handle);
  EXPECT_FALSE(task_->HasActorSurface(actor_surface_handle));
  EXPECT_FALSE(task_->IsActingOnActorSurface(actor_surface_handle));
  EXPECT_TRUE(task_->GetActorSurfaces().empty());
  // The last actuated surface is preserved after removal.
  EXPECT_EQ(task_->GetLastActuatedActorSurface(), actor_surface_handle);

  // Observing a surface adds it to the last-acted set only.
  task_->ObserveActorSurfaceOnce(actor_surface_handle);
  EXPECT_FALSE(task_->HasActorSurface(actor_surface_handle));
  EXPECT_THAT(task_->GetLastActedActorSurfaces(),
              ::testing::UnorderedElementsAre(actor_surface_handle));
}

TEST_F(ActorTaskSurfaceTest, ObserveTabOnceThroughSurface) {
  task_->ObserveTabOnce(mock_tab_.GetHandle());
  EXPECT_TRUE(registry().GetForTab(mock_tab_.GetHandle()));
  EXPECT_TRUE(web_contents_->IsBeingCaptured());

  task_->Pause(/*from_actor=*/false);
  EXPECT_FALSE(web_contents_->IsBeingCaptured());
}

// A surface without a tab comes under actor control like a tab, but doesn't
// count as visible.
TEST_F(ActorTaskSurfaceTest, ControlsSurfaceWithoutTab) {
  ScopedHeadlessActorSurface headless = service().CreateHeadlessActorSurface();
  const ActorSurfaceHandle actor_surface_handle =
      headless.actor_surface_handle();
  ActorSurface* actor_surface = actor_surface_handle.Get();
  ASSERT_TRUE(actor_surface);
  ASSERT_FALSE(actor_surface->GetTabHandle());
  content::WebContents* contents = actor_surface->GetWebContents();

  base::test::TestFuture<mojom::ActionResultPtr> add_future;
  task_->AddActorSurface(actor_surface_handle, /*stop_task_on_detach=*/true,
                         add_future.GetCallback());
  ASSERT_TRUE(add_future.Wait());
  EXPECT_TRUE(task_->IsActingOnActorSurface(actor_surface_handle));
  EXPECT_TRUE(contents->IsBeingCaptured());
  EXPECT_EQ(task_->GetUiMode(), ActorUiMode::kBackground);

  // Pausing and resuming exits and re-enters actor control.
  task_->Pause(/*from_actor=*/false);
  EXPECT_FALSE(contents->IsBeingCaptured());
  task_->Resume();
  EXPECT_TRUE(contents->IsBeingCaptured());

  task_->RemoveActorSurface(actor_surface_handle);
  EXPECT_FALSE(contents->IsBeingCaptured());
}

// A page without a tab can't close itself (e.g. window.close()), so the task
// keeps acting on it, unlike a tab-backed surface whose tab closes.
TEST_F(ActorTaskSurfaceTest, SurfaceWithoutTabClosingItselfKeepsTask) {
  ScopedHeadlessActorSurface headless = service().CreateHeadlessActorSurface();
  const ActorSurfaceHandle actor_surface_handle =
      headless.actor_surface_handle();
  content::WebContents* contents = actor_surface_handle.Get()->GetWebContents();

  base::test::TestFuture<mojom::ActionResultPtr> add_future;
  task_->AddActorSurface(actor_surface_handle, /*stop_task_on_detach=*/true,
                         add_future.GetCallback());
  ASSERT_TRUE(add_future.Wait());

  contents->Close();

  EXPECT_FALSE(task_->IsCompleted());
  EXPECT_TRUE(task_->IsActingOnActorSurface(actor_surface_handle));
  EXPECT_TRUE(registry().Get(actor_surface_handle));

  task_->RemoveActorSurface(actor_surface_handle);
}

// Dropping the ScopedHeadlessActorSurface destroys the page. A task still
// acting on it stops instead of keeping a surface that is gone.
TEST_F(ActorTaskSurfaceTest, DestroyingSurfaceWithoutTabStopsTask) {
  std::optional<ScopedHeadlessActorSurface> headless =
      service().CreateHeadlessActorSurface();
  const ActorSurfaceHandle actor_surface_handle =
      headless->actor_surface_handle();
  content::WebContentsDestroyedWatcher watcher(
      actor_surface_handle.Get()->GetWebContents());

  base::test::TestFuture<mojom::ActionResultPtr> add_future;
  task_->AddActorSurface(actor_surface_handle, /*stop_task_on_detach=*/true,
                         add_future.GetCallback());
  ASSERT_TRUE(add_future.Wait());

  ExpectStopNotification(ActorTask::State::kCancelled);
  ExpectTabRemovedNotification(tabs::TabHandle::Null());
  headless.reset();

  EXPECT_TRUE(task_->IsCompleted());
  EXPECT_EQ(task_->stopped_reason(), ActorTask::StoppedReason::kTabDetached);
  EXPECT_FALSE(registry().Get(actor_surface_handle));
  EXPECT_TRUE(watcher.IsDestroyed());
}

// A task stopping because its page is being destroyed may ask to destroy the
// same page again, e.g. from a task state callback. That nested request is
// ignored: the page stays alive until every task has been told.
TEST_F(ActorTaskSurfaceTest, DestroyingSurfaceWithoutTabAgainWhileNotifying) {
  std::optional<ScopedHeadlessActorSurface> headless =
      service().CreateHeadlessActorSurface();
  const ActorSurfaceHandle actor_surface_handle =
      headless->actor_surface_handle();
  content::WebContentsDestroyedWatcher watcher(
      actor_surface_handle.Get()->GetWebContents());

  base::test::TestFuture<mojom::ActionResultPtr> add_future;
  task_->AddActorSurface(actor_surface_handle, /*stop_task_on_detach=*/true,
                         add_future.GetCallback());
  ASSERT_TRUE(add_future.Wait());

  bool destroyed_again = false;
  base::CallbackListSubscription subscription =
      service().AddTaskStateChangedCallback(
          base::BindLambdaForTesting([&](ActorTask& task) {
            if (!task.IsCompleted()) {
              return;
            }
            service().DestroyHeadlessActorSurface(actor_surface_handle);
            destroyed_again = true;
            EXPECT_FALSE(watcher.IsDestroyed());
            EXPECT_TRUE(registry().Get(actor_surface_handle));
          }));

  ExpectStopNotification(ActorTask::State::kCancelled);
  ExpectTabRemovedNotification(tabs::TabHandle::Null());
  headless.reset();

  EXPECT_TRUE(destroyed_again);
  EXPECT_TRUE(task_->IsCompleted());
  EXPECT_TRUE(watcher.IsDestroyed());
}

// Shutdown() destroys every headless page; a ScopedHeadlessActorSurface dropped
// afterwards does nothing.
TEST_F(ActorTaskSurfaceTest, DroppingSurfaceWithoutTabAfterShutdown) {
  std::optional<ScopedHeadlessActorSurface> headless =
      service().CreateHeadlessActorSurface();
  content::WebContentsDestroyedWatcher watcher(
      headless->actor_surface_handle().Get()->GetWebContents());

  // Shutdown() deletes the task, which owns the mock dispatcher. Drop the
  // pointers to them first so they don't dangle.
  task_ = nullptr;
  mock_ui_event_dispatcher_ = nullptr;
  service().Shutdown();
  EXPECT_TRUE(watcher.IsDestroyed());

  headless.reset();
}

// A call whose tab is still being added is superseded by a later call, so only
// the later call's `on_actions_accepted` runs.
TEST_F(ActorTaskTest, OnActionsAcceptedNotRunWhenSuperseded) {
  // The first call waits for its tab to be added, which completes
  // asynchronously. Its actions never start, so the tab needs no WebContents.
  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetProfile)
      .WillByDefault(::testing::Return(profile_.get()));
  registry().OnTabCreated(mock_tab);
  ActResultFuture first_future;
  task_->Act(MakeActionsNotExpectedToStart(
                 ActorSurfaceHandle::From(mock_tab.GetHandle())),
             base::MakeExpectedNotRunClosure(FROM_HERE),
             first_future.GetCallback());

  // The second call has no tab to add, so it starts its actions right away.
  // Not running actions on a tab keeps its WebContents from being captured,
  // which would take a wake lock whose asynchronous D-Bus reply can arrive
  // after the test ends.
  std::vector<std::unique_ptr<ToolRequest>> second_actions;
  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  second_actions.push_back(
      std::make_unique<FakeToolRequest>(on_invoke_future.GetCallback(),
                                        /*on_destroy=*/base::DoNothing()));
  base::test::TestFuture<void> second_accepted;
  ActResultFuture second_future;
  task_->Act(std::move(second_actions), second_accepted.GetCallback(),
             second_future.GetCallback());

  ASSERT_TRUE(on_invoke_future.Wait());
  EXPECT_TRUE(second_accepted.IsReady());
  std::move(on_invoke_future.Take()).Run(MakeOkResult());
  ASSERT_TRUE(second_future.Wait());
  EXPECT_EQ(second_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);

  // The first call's tab addition was posted before anything the second call
  // posted, so it has completed by now. Its actions still never start.
  EXPECT_FALSE(first_future.IsReady());

  // Stop the task while `mock_tab` is still alive.
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
}

TEST_F(ActorTaskTest, AddTab_RejectsTabOwnedByAnotherActiveOrPausedTask) {
  tabs::MockTabInterface tab;
  AddTabAndVerify(tab);

  EXPECT_EQ(AddTabAndGetCode(*task_, tab.GetHandle()),
            mojom::ActionResultCode::kOk);

  ui::MockUiEventDispatcher* second_dispatcher = nullptr;
  ActorTask* second_task = CreateAdditionalTask(&second_dispatcher);
  ASSERT_NE(second_task, nullptr);
  EXPECT_CALL(
      *second_dispatcher,
      OnActorTaskAsyncChange(
          VariantWith<ui::UiEventDispatcher::AddTab>(Field(
              &ui::UiEventDispatcher::AddTab::handle, Eq(tab.GetHandle()))),
          _))
      .Times(0);

  EXPECT_EQ(AddTabAndGetCode(*second_task, tab.GetHandle()),
            mojom::ActionResultCode::kTabOwnedByAnotherTask);
  EXPECT_FALSE(second_task->HasTab(tab.GetHandle()));
  EXPECT_TRUE(second_task->GetTabs().empty());
  EXPECT_EQ(second_task->GetLastActuatedTabHandle(), tabs::TabHandle::Null());

  ExpectStateChangeNotification(ActorTask::State::kPausedByActor);
  task_->Pause(/*from_actor=*/true);
  ASSERT_TRUE(task_->IsUnderUserControl());

  EXPECT_EQ(AddTabAndGetCode(*second_task, tab.GetHandle()),
            mojom::ActionResultCode::kTabOwnedByAnotherTask);
  EXPECT_FALSE(second_task->HasTab(tab.GetHandle()));
}

TEST_F(ActorTaskSurfaceTest,
       AddActorSurface_RejectsHeadlessSurfaceOwnedByAnotherTask) {
  ScopedHeadlessActorSurface headless = service().CreateHeadlessActorSurface();
  const ActorSurfaceHandle actor_surface_handle =
      headless.actor_surface_handle();

  base::test::TestFuture<mojom::ActionResultPtr> first_future;
  task_->AddActorSurface(actor_surface_handle, /*stop_task_on_detach=*/true,
                         first_future.GetCallback());
  ExpectOkResult(first_future);

  ActorTask* second_task = CreateAdditionalTask();
  ASSERT_NE(second_task, nullptr);

  base::test::TestFuture<mojom::ActionResultPtr> second_future;
  second_task->AddActorSurface(actor_surface_handle,
                               /*stop_task_on_detach=*/true,
                               second_future.GetCallback());
  EXPECT_EQ(second_future.Take()->code,
            mojom::ActionResultCode::kTabOwnedByAnotherTask);
  EXPECT_FALSE(second_task->HasActorSurface(actor_surface_handle));
}

TEST_F(ActorTaskTest, Act_RejectsTabOwnedByAnotherTaskOnTurn1AndTurn2) {
  auto web_contents1 = CreateTestWebContents();
  content::WebContentsTester::For(web_contents1.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState tab1(web_contents1.get());
  AddTabAndVerify(tab1.tab);

  auto web_contents2 = CreateTestWebContents();
  content::WebContentsTester::For(web_contents2.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState tab2(web_contents2.get());

  ActorTask* second_task = CreateAdditionalTask();
  ASSERT_NE(second_task, nullptr);
  ASSERT_EQ(second_task->GetState(), ActorTask::State::kCreated);

  bool turn1_invoked = false;
  EXPECT_EQ(
      ActOnTabAndGetCode(*second_task, tab1.tab.GetHandle(), &turn1_invoked),
      mojom::ActionResultCode::kTabOwnedByAnotherTask);
  EXPECT_FALSE(turn1_invoked);
  EXPECT_EQ(second_task->GetState(), ActorTask::State::kCreated);
  EXPECT_FALSE(second_task->HasTab(tab1.tab.GetHandle()));

  EXPECT_EQ(ActOnTabAndGetCode(*second_task, tab2.tab.GetHandle()),
            mojom::ActionResultCode::kOk);
  EXPECT_EQ(second_task->GetState(), ActorTask::State::kReflecting);
  EXPECT_TRUE(second_task->HasTab(tab2.tab.GetHandle()));

  bool turn2_invoked = false;
  EXPECT_EQ(
      ActOnTabAndGetCode(*second_task, tab1.tab.GetHandle(), &turn2_invoked),
      mojom::ActionResultCode::kTabOwnedByAnotherTask);
  EXPECT_FALSE(turn2_invoked);
  EXPECT_EQ(second_task->GetState(), ActorTask::State::kReflecting);
  EXPECT_FALSE(second_task->HasTab(tab1.tab.GetHandle()));
}

TEST_F(ActorTaskTest, Pause_WhileAddingTabsInReflecting) {
  auto web_contents = CreateTestWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState tab_state(web_contents.get());
  tabs::TabHandle tab_handle = tab_state.tab.GetHandle();

  ExpectTabAddedNotification(tab_handle);
  EXPECT_EQ(ActOnTabAndGetCode(*task_, tab_handle),
            mojom::ActionResultCode::kOk);
  ASSERT_EQ(task_->GetState(), ActorTask::State::kReflecting);

  std::vector<std::unique_ptr<ToolRequest>> actions;
  actions.push_back(std::make_unique<FakeToolRequest>(
      base::DoNothing(), /*on_destroy=*/base::DoNothing(),
      ActorSurfaceHandle::From(tab_handle)));
  ActResultFuture act_future;
  task_->Act(std::move(actions), act_future.GetCallback());
  task_->Pause(/*from_actor=*/true);
  ASSERT_TRUE(act_future.Wait());
  EXPECT_EQ(act_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskPaused);
}

TEST_F(ActorTaskTest, Stop_WhileAddingTabsInReflecting) {
  auto web_contents = CreateTestWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState tab_state(web_contents.get());
  tabs::TabHandle tab_handle = tab_state.tab.GetHandle();

  ExpectTabAddedNotification(tab_handle);
  EXPECT_EQ(ActOnTabAndGetCode(*task_, tab_handle),
            mojom::ActionResultCode::kOk);
  ASSERT_EQ(task_->GetState(), ActorTask::State::kReflecting);

  std::vector<std::unique_ptr<ToolRequest>> actions;
  actions.push_back(std::make_unique<FakeToolRequest>(
      base::DoNothing(), /*on_destroy=*/base::DoNothing(),
      ActorSurfaceHandle::From(tab_handle)));
  ActResultFuture act_future;
  task_->Act(std::move(actions), act_future.GetCallback());
  task_->Stop(ActorTask::StoppedReason::kTaskComplete);
  ASSERT_TRUE(act_future.Wait());
  EXPECT_EQ(act_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskWentAway);
}

TEST_F(ActorTaskTest, AddTab_SucceedsAfterOwningTaskRemovesTabOrStops) {
  tabs::MockTabInterface tab;
  AddTabAndVerify(tab);

  ActorTask* second_task = CreateAdditionalTask();
  ASSERT_NE(second_task, nullptr);

  RemoveTabAndVerify(tab);

  EXPECT_EQ(AddTabAndGetCode(*second_task, tab.GetHandle()),
            mojom::ActionResultCode::kOk);
  EXPECT_TRUE(second_task->HasTab(tab.GetHandle()));

  ActorTask* third_task = CreateAdditionalTask();
  ASSERT_NE(third_task, nullptr);

  ActorKeyedService::Get(profile_.get())
      ->StopTask(second_task->id(), ActorTask::StoppedReason::kTaskComplete);

  EXPECT_EQ(AddTabAndGetCode(*third_task, tab.GetHandle()),
            mojom::ActionResultCode::kOk);
  EXPECT_TRUE(third_task->HasTab(tab.GetHandle()));
}

TEST_F(ActorTaskTest, Act_StaysActingWhenCallbackStartsAnotherAct) {
  base::test::TestFuture<DeprecatedToolCallback> second_invoke_future;
  ActResultFuture second_future;

  std::vector<std::unique_ptr<ToolRequest>> first_actions;
  first_actions.push_back(std::make_unique<FakeToolRequest>(
      base::BindOnce([](DeprecatedToolCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }),
      /*on_destroy=*/base::DoNothing()));
  base::test::TestFuture<void> first_finished;
  task_->Act(std::move(first_actions),
             base::BindLambdaForTesting(
                 [&](std::vector<ToolResultWithLatencyInfo> results,
                     TabObservationStrategy strategy) {
                   std::vector<std::unique_ptr<ToolRequest>> second_actions;
                   second_actions.push_back(std::make_unique<FakeToolRequest>(
                       second_invoke_future.GetCallback(),
                       /*on_destroy=*/base::DoNothing()));
                   task_->Act(std::move(second_actions),
                              second_future.GetCallback());
                   first_finished.SetValue();
                 }));
  ASSERT_TRUE(first_finished.Wait());

  ASSERT_TRUE(second_invoke_future.Wait());
  EXPECT_EQ(task_->GetState(), ActorTask::State::kActing);

  second_invoke_future.Take().Run(MakeOkResult());
  ASSERT_TRUE(second_future.Wait());
  EXPECT_EQ(second_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
  EXPECT_EQ(task_->GetState(), ActorTask::State::kReflecting);
}

TEST_F(ActorTaskTest, Act_RemovesTabsOnCompletionWhenNotRetainingTabs) {
  auto web_contents = CreateTestWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState tab_state(web_contents.get());
  tabs::TabHandle tab_handle = tab_state.tab.GetHandle();

  ActorTaskOptions options;
  options.retain_tabs_after_act = false;
  ActorTask* non_retaining_task =
      CreateAdditionalTask(/*out_dispatcher=*/nullptr, std::move(options));
  ASSERT_NE(non_retaining_task, nullptr);

  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  std::vector<std::unique_ptr<ToolRequest>> actions;
  actions.push_back(std::make_unique<FakeToolRequest>(
      on_invoke_future.GetCallback(), /*on_destroy=*/base::DoNothing(),
      ActorSurfaceHandle::From(tab_handle)));

  bool had_tab_in_callback = true;
  ActResultFuture act_future;
  non_retaining_task->Act(
      std::move(actions),
      base::BindLambdaForTesting(
          [&](std::vector<ToolResultWithLatencyInfo> results,
              TabObservationStrategy strategy) {
            had_tab_in_callback = non_retaining_task->HasTab(tab_handle);
            act_future.GetCallback().Run(std::move(results),
                                         std::move(strategy));
          }));
  ASSERT_TRUE(on_invoke_future.Wait());
  EXPECT_TRUE(non_retaining_task->IsActingOnTab(tab_handle));
  EXPECT_TRUE(web_contents->IsBeingCaptured());
  EXPECT_EQ(AddTabAndGetCode(*task_, tab_handle),
            mojom::ActionResultCode::kTabOwnedByAnotherTask);

  on_invoke_future.Take().Run(MakeOkResult());
  ASSERT_TRUE(act_future.Wait());
  EXPECT_EQ(act_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kOk);
  EXPECT_FALSE(had_tab_in_callback);
  EXPECT_EQ(non_retaining_task->GetState(), ActorTask::State::kReflecting);
  EXPECT_TRUE(non_retaining_task->GetTabs().empty());
  EXPECT_FALSE(web_contents->IsBeingCaptured());

  ExpectTabAddedNotification(tab_handle);
  EXPECT_EQ(AddTabAndGetCode(*task_, tab_handle), mojom::ActionResultCode::kOk);
  EXPECT_EQ(ActOnTabAndGetCode(*non_retaining_task, tab_handle),
            mojom::ActionResultCode::kTabOwnedByAnotherTask);
}

TEST_F(ActorTaskTest, Act_RemovesAddedTabsOnEarlyAddFailureWhenNotRetaining) {
  tabs::MockTabInterface owned_tab;
  AddTabAndVerify(owned_tab);

  auto web_contents = CreateTestWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState free_tab(web_contents.get());

  ActorTaskOptions options;
  options.retain_tabs_after_act = false;
  ActorTask* non_retaining_task =
      CreateAdditionalTask(/*out_dispatcher=*/nullptr, std::move(options));
  ASSERT_NE(non_retaining_task, nullptr);

  std::vector<std::unique_ptr<ToolRequest>> actions;
  for (tabs::TabHandle tab_handle :
       {free_tab.tab.GetHandle(), owned_tab.GetHandle()}) {
    actions.push_back(std::make_unique<FakeToolRequest>(
        base::BindRepeating(
            [](DeprecatedToolCallback) { ADD_FAILURE() << "Tool invoked"; }),
        /*on_destroy=*/base::DoNothing(),
        ActorSurfaceHandle::From(tab_handle)));
  }
  ActResultFuture act_future;
  non_retaining_task->Act(std::move(actions), act_future.GetCallback());
  ASSERT_TRUE(act_future.Wait());
  EXPECT_EQ(act_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTabOwnedByAnotherTask);
  EXPECT_TRUE(non_retaining_task->GetTabs().empty());
  EXPECT_FALSE(web_contents->IsBeingCaptured());
}

TEST_F(ActorTaskTest, Act_ClearsAdditionalTabObservationsWhenNotRetaining) {
  base::test::ScopedFeatureList feature_list(
      kGlicActorLoadAndExtractContentTool);

  ActorTaskOptions options;
  options.retain_tabs_after_act = false;
  ActorTask* non_retaining_task =
      CreateAdditionalTask(/*out_dispatcher=*/nullptr, std::move(options));
  ASSERT_NE(non_retaining_task, nullptr);

  for (ActorTask* task : {task_.get(), non_retaining_task}) {
    std::vector<std::unique_ptr<ToolRequest>> actions;
    actions.push_back(std::make_unique<FakeToolRequest>(
        base::BindLambdaForTesting([task](DeprecatedToolCallback callback) {
          task->AddAdditionalTabObservations(
              {optimization_guide::proto::TabObservation()});
          std::move(callback).Run(MakeOkResult());
        }),
        /*on_destroy=*/base::DoNothing()));
    ActResultFuture act_future;
    task->Act(std::move(actions), act_future.GetCallback());
    ASSERT_TRUE(act_future.Wait());
  }

  EXPECT_EQ(task_->GetAdditionalTabObservations().size(), 1u);
  EXPECT_TRUE(non_retaining_task->GetAdditionalTabObservations().empty());
}

TEST_F(ActorTaskTest, Pause_RemovesTabsWhenNotRetainingTabs) {
  auto web_contents = CreateTestWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState tab_state(web_contents.get());
  tabs::TabHandle tab_handle = tab_state.tab.GetHandle();

  ActorTaskOptions options;
  options.retain_tabs_after_act = false;
  ActorTask* non_retaining_task =
      CreateAdditionalTask(/*out_dispatcher=*/nullptr, std::move(options));
  ASSERT_NE(non_retaining_task, nullptr);

  base::test::TestFuture<DeprecatedToolCallback> on_invoke_future;
  std::vector<std::unique_ptr<ToolRequest>> actions;
  actions.push_back(std::make_unique<FakeToolRequest>(
      on_invoke_future.GetCallback(), /*on_destroy=*/base::DoNothing(),
      ActorSurfaceHandle::From(tab_handle)));

  ActResultFuture act_future;
  non_retaining_task->Act(std::move(actions), act_future.GetCallback());
  ASSERT_TRUE(on_invoke_future.Wait());
  EXPECT_TRUE(non_retaining_task->HasTab(tab_handle));

  non_retaining_task->Pause(/*from_actor=*/true);
  ASSERT_TRUE(act_future.Wait());
  EXPECT_EQ(act_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskPaused);
  EXPECT_FALSE(non_retaining_task->HasTab(tab_handle));
  EXPECT_FALSE(web_contents->IsBeingCaptured());

  non_retaining_task->Resume();
  EXPECT_FALSE(non_retaining_task->HasTab(tab_handle));
  EXPECT_FALSE(web_contents->IsBeingCaptured());
}

TEST_F(ActorTaskTest, Pause_RemovesTabsBeingAddedWhenNotRetainingTabs) {
  auto web_contents = CreateTestWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("about:blank"));
  TestTabState tab_state(web_contents.get());
  tabs::TabHandle tab_handle = tab_state.tab.GetHandle();

  ActorTaskOptions options;
  options.retain_tabs_after_act = false;
  ActorTask* non_retaining_task =
      CreateAdditionalTask(/*out_dispatcher=*/nullptr, std::move(options));
  ASSERT_NE(non_retaining_task, nullptr);

  std::vector<std::unique_ptr<ToolRequest>> actions;
  actions.push_back(std::make_unique<FakeToolRequest>(
      base::DoNothing(), /*on_destroy=*/base::DoNothing(),
      ActorSurfaceHandle::From(tab_handle)));
  ActResultFuture act_future;
  non_retaining_task->Act(std::move(actions), act_future.GetCallback());
  ASSERT_EQ(non_retaining_task->GetState(), ActorTask::State::kCreated);
  ASSERT_TRUE(non_retaining_task->HasTab(tab_handle));

  non_retaining_task->Pause(/*from_actor=*/true);
  ASSERT_TRUE(act_future.Wait());
  EXPECT_EQ(act_future.Get()[0].tool_result.deprecated_action_result()->code,
            mojom::ActionResultCode::kTaskPaused);
  EXPECT_FALSE(non_retaining_task->HasTab(tab_handle));
}

}  // namespace

}  // namespace actor

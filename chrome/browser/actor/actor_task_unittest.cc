// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/fake_tool_request.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/task_source_info.h"
#include "components/tabs/public/mock_tab_interface.h"
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
    scoped_feature_list_.InitAndEnableFeature(kActorFormScriptToolInterrupt);
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    // Setup ExecutionEngine mock dispatcher.
    auto ee_mock_ui_event_dispatcher =
        std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
    mock_ee_ui_event_dispatcher_ = ee_mock_ui_event_dispatcher.get();

    ON_CALL(*mock_ee_ui_event_dispatcher_, OnPreTool)
        .WillByDefault([](const ToolRequest&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });

    ON_CALL(*mock_ee_ui_event_dispatcher_, OnPostTool)
        .WillByDefault([](const ToolRequest&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });

    scoped_ee_factory_ = std::make_unique<ScopedExecutionEngineFactory>(
        base::BindLambdaForTesting([&](actor::ActorTask& task) {
          return actor::ExecutionEngine::CreateForTesting(
              task, std::move(ee_mock_ui_event_dispatcher));
        }));

    // Setup ActorTask mock dispatcher.
    auto mock_ui_event_dispatcher =
        std::make_unique<testing::NiceMock<ui::MockUiEventDispatcher>>();
    mock_ui_event_dispatcher_ = mock_ui_event_dispatcher.get();

    ON_CALL(*mock_ui_event_dispatcher_, OnActorTaskAsyncChange)
        .WillByDefault([](const ui::UiEventDispatcher::ActorTaskAsyncChange&,
                          ui::UiEventDispatcher::UiCompleteCallback callback) {
          std::move(callback).Run(MakeOkResult());
        });

    task_ = ActorTask::CreateForTesting(
        *ActorKeyedService::Get(profile_.get()), TaskId(1),
        std::move(mock_ui_event_dispatcher),
        /*options=*/nullptr, TestTaskSourceInfo(), NoEnterprisePolicyChecker(),
        mock_delegate_.GetWeakPtr());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  MockActorTaskDelegate mock_delegate_;
  std::unique_ptr<ScopedExecutionEngineFactory> scoped_ee_factory_;
  std::unique_ptr<ActorTask> task_;
  raw_ptr<ui::MockUiEventDispatcher> mock_ui_event_dispatcher_;
  raw_ptr<ui::MockUiEventDispatcher> mock_ee_ui_event_dispatcher_;
};

// TODO(crbug.com/512551823): Re-enable tests when crash in
// ActorTask::CreateForTesting is fixed
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_CustomToolInterruptsWithUserControl \
  DISABLED_CustomToolInterruptsWithUserControl
#else
#define MAYBE_CustomToolInterruptsWithUserControl \
  CustomToolInterruptsWithUserControl
#endif
TEST_F(ActorTaskTest, MAYBE_CustomToolInterruptsWithUserControl) {
  // 1. Add a tab to the task.
  tabs::MockTabInterface mock_tab;
  AddTabToTask(mock_tab, *task_);

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
  EXPECT_EQ(task_->GetState(), ActorTask::State::kWaitingOnUser);
  EXPECT_TRUE(task_->IsUnderUserControl());
  EXPECT_FALSE(task_->IsUnderActorControl());

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

}  // namespace

}  // namespace actor

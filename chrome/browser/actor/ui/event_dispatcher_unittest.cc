// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/mock_actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/common/actor/action_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {

namespace {

using ::actor::mojom::ActionResultPtr;
using base::test::TestFuture;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;
using testing::VariantWith;
using testing::WithArgs;

class EventDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_state_manager_ = std::make_unique<MockActorUiStateManager>();
    dispatcher_ = NewUiEventDispatcher(mock_state_manager_.get());
  }

  std::unique_ptr<MockActorUiStateManager> mock_state_manager_;
  std::unique_ptr<UiEventDispatcher> dispatcher_;
};

TEST_F(EventDispatcherTest, NoEventsToDispatch) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(_, _)).Times(0);
  WaitToolRequest tr(base::Microseconds(1000));
  TestFuture<ActionResultPtr> success;
  dispatcher_->OnPostTool(tr, success.GetCallback());
  EXPECT_TRUE(IsOk(*success.Get()));
}

TEST_F(EventDispatcherTest, SingleUiEvent) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  MoveMouseToolRequest tr(tabs::TabHandle(123),
                          PageTarget(gfx::Point(100, 200)));
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

TEST_F(EventDispatcherTest, TwoToolRequests) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .Times(2)
      .WillRepeatedly(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  MoveMouseToolRequest tr1(tabs::TabHandle(123),
                           PageTarget(gfx::Point(100, 200)));
  MoveMouseToolRequest tr2(tabs::TabHandle(456),
                           PageTarget(gfx::Point(300, 400)));
  TestFuture<ActionResultPtr> result1, result2;
  dispatcher_->OnPreTool(tr1, result1.GetCallback());
  dispatcher_->OnPreTool(tr2, result2.GetCallback());
  EXPECT_TRUE(IsOk(*result1.Get()));
  EXPECT_TRUE(IsOk(*result2.Get()));
}

TEST_F(EventDispatcherTest, TwoUiEvents) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  ClickToolRequest tr(tabs::TabHandle(123), PageTarget(gfx::Point(10, 50)),
                      MouseClickType::kLeft, MouseClickCount::kSingle);
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

TEST_F(EventDispatcherTest, TwoUiEventsWithFirstOneFailing) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeErrorResult());
      }));
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseClick>(_), _))
      .Times(0);
  ClickToolRequest tr(tabs::TabHandle(123), PageTarget(gfx::Point(10, 50)),
                      MouseClickType::kLeft, MouseClickCount::kSingle);
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_EQ(result.Get()->code, ::actor::mojom::ActionResultCode::kError);
}

TEST_F(EventDispatcherTest, TypeCausesMouseMove) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<MouseMove>(_), _))
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  TypeToolRequest tr(tabs::TabHandle(456), PageTarget(gfx::Point(300, 400)),
                     "some text to type",
                     /*follow_by_enter=*/true, TypeToolRequest::Mode::kReplace);
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnPreTool(tr, result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

TEST_F(EventDispatcherTest, SyncActorTaskChange_OneEvent) {
  EXPECT_CALL(
      *mock_state_manager_,
      OnUiEvent(VariantWith<TaskStateChanged>(AllOf(
          Field(&TaskStateChanged::task_id, TaskId(999)),
          Field(&TaskStateChanged::state, ActorTask::State::kPausedByClient)))))
      .Times(1);
  dispatcher_->OnActorTaskSyncChange(UiEventDispatcher::ChangeTaskState{
      .task_id = TaskId(999),
      .old_state = ActorTask::State::kActing,
      .new_state = ActorTask::State::kPausedByClient});
}

TEST_F(EventDispatcherTest, SyncActorTaskChange_NewTask) {
  EXPECT_CALL(*mock_state_manager_, OnUiEvent(VariantWith<StartTask>(Field(
                                        &StartTask::task_id, TaskId(222)))))
      .Times(1);
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<TaskStateChanged>(AllOf(
                  Field(&TaskStateChanged::task_id, TaskId(222)),
                  Field(&TaskStateChanged::state, ActorTask::State::kActing)))))
      .Times(1);
  dispatcher_->OnActorTaskSyncChange(UiEventDispatcher::ChangeTaskState{
      .task_id = TaskId(222),
      .old_state = ActorTask::State::kCreated,
      .new_state = ActorTask::State::kActing});
}

TEST_F(EventDispatcherTest, SyncActor_RemoveTab) {
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<StoppedActingOnTab>(Field(
                  &StoppedActingOnTab::tab_handle, tabs::TabHandle(5309)))))
      .Times(1);
  dispatcher_->OnActorTaskSyncChange(UiEventDispatcher::RemoveTab{
      .task_id = TaskId(867), .handle = tabs::TabHandle(5309)});
}

TEST_F(EventDispatcherTest, AsyncActorTaskChange_OneEvent) {
  EXPECT_CALL(*mock_state_manager_,
              OnUiEvent(VariantWith<StartingToActOnTab>(_), _))
      .Times(1)
      .WillOnce(WithArgs<1>([&](UiCompleteCallback callback) {
        std::move(callback).Run(MakeOkResult());
      }));
  TestFuture<ActionResultPtr> result;
  dispatcher_->OnActorTaskAsyncChange(
      UiEventDispatcher::AddTab{.task_id = TaskId(992),
                                .handle = tabs::TabHandle(998)},
      result.GetCallback());
  EXPECT_TRUE(IsOk(*result.Get()));
}

// TODO(crbug.com/425784083): improve unit testing

}  // namespace

}  // namespace actor::ui

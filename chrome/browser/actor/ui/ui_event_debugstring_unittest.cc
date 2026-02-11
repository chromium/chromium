// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/ui_event_debugstring.h"

#include "chrome/browser/actor/ui/ui_event.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {
using testing::Return;

class UiEventDebugStringTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(tab_interface_, GetTabHandle()).WillByDefault(Return(5555));
  }

  tabs::TabInterface::Handle Handle() {
    return tabs::TabInterface::Handle(tab_interface_.GetTabHandle());
  }

  tabs::MockTabInterface tab_interface_;
};

TEST_F(UiEventDebugStringTest, StartTask) {
  EXPECT_EQ(DebugString(UiEvent(StartTask(TaskId(123)))), "StartTask[id=123]");
}

TEST_F(UiEventDebugStringTest, StopTask) {
  EXPECT_EQ(
      DebugString(SyncUiEvent(StopTask(
          TaskId(123), ActorTask::State::kCancelled, /*title=*/"", Handle()))),
      "StopTask[id=123, final_state=Cancelled, title="
      ", last_acted_on_tab=5555]");
}

TEST_F(UiEventDebugStringTest, TaskStateChanged) {
  EXPECT_EQ(DebugString(SyncUiEvent(
                TaskStateChanged(TaskId(123), ActorTask::State::kActing))),
            "TaskStateChanged[task_id=123, state=Acting]");
  EXPECT_EQ(DebugString(UiEvent(TaskStateChanged(
                TaskId(8675), ActorTask::State::kPausedByActor))),
            "TaskStateChanged[task_id=8675, state=PausedByActor]");
}

TEST_F(UiEventDebugStringTest, StartingToActOnTab) {
  EXPECT_EQ(DebugString(UiEvent(StartingToActOnTab(Handle(), TaskId(123)))),
            "StartingToActOnTab[task_id=123, tab=5555]");
}

TEST_F(UiEventDebugStringTest, StoppedActingOnTab) {
  EXPECT_EQ(DebugString(UiEvent(StoppedActingOnTab(Handle()))),
            "StoppedActingOnTab[tab=5555]");
}

TEST_F(UiEventDebugStringTest, MouseMove) {
  EXPECT_EQ(DebugString(UiEvent(MouseMove(Handle(), gfx::Point(10, 20),
                                          TargetSource::kToolRequest))),
            "MouseMove[target=10,20 target_source=ToolRequest]");
  EXPECT_EQ(DebugString(AsyncUiEvent(MouseMove(
                Handle(), std::nullopt, TargetSource::kUnresolvableInApc))),
            "MouseMove[target=null target_source=UnresolvableInApc]");
  EXPECT_EQ(DebugString(UiEvent(MouseMove(Handle(), gfx::Point(999, 888),
                                          TargetSource::kDerivedFromApc))),
            "MouseMove[target=999,888 target_source=DerivedFromApc]");
}

TEST_F(UiEventDebugStringTest, MouseClick) {
  EXPECT_EQ(DebugString(UiEvent(MouseClick(Handle(), mojom::ClickType::kLeft,
                                           mojom::ClickCount::kSingle))),
            "MouseClick[type=kLeft, count=kSingle]");
  EXPECT_EQ(
      DebugString(AsyncUiEvent(MouseClick(Handle(), mojom::ClickType::kRight,
                                          mojom::ClickCount::kDouble))),
      "MouseClick[type=kRight, count=kDouble]");
}

TEST_F(UiEventDebugStringTest, MouseMove_UnresolvableFromRenderer) {
  EXPECT_EQ(
      DebugString(UiEvent(MouseMove(Handle(), std::nullopt,
                                    TargetSource::kUnresolvableFromRenderer))),
      "MouseMove[target=null target_source=UnresolvableFromRenderer]");
}

TEST_F(UiEventDebugStringTest, MouseMove_RendererResolved) {
  EXPECT_EQ(DebugString(UiEvent(MouseMove(Handle(), gfx::Point(50, 50),
                                          TargetSource::kRendererResolved))),
            "MouseMove[target=50,50 target_source=RendererResolved]");
}

}  // namespace
}  // namespace actor::ui

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

constexpr PageTarget PointTarget() {
  return gfx::Point(10, 20);
}

constexpr PageTarget DomNodeTarget() {
  return DomNode{.node_id = 30,
                 .document_identifier = "some_document_identifier"};
}

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

TEST_F(UiEventDebugStringTest, TaskStateChanged) {
  EXPECT_EQ(DebugString(SyncUiEvent(
                TaskStateChanged(TaskId(123), ActorTask::State::kActing))),
            "TaskStateChanged[task_id=123, state=Acting]");
  EXPECT_EQ(DebugString(UiEvent(TaskStateChanged(
                TaskId(8675), ActorTask::State::kPausedByClient))),
            "TaskStateChanged[task_id=8675, state=PausedByClient]");
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
  EXPECT_EQ(DebugString(UiEvent(MouseMove(Handle(), PointTarget()))),
            "MouseMove[target=10,20]");
  EXPECT_EQ(DebugString(AsyncUiEvent(MouseMove(Handle(), DomNodeTarget()))),
            "MouseMove[target=DomNode[id=30 doc_id=some_document_identifier]]");
}

TEST_F(UiEventDebugStringTest, MouseClick) {
  EXPECT_EQ(DebugString(UiEvent(MouseClick(Handle(), MouseClickType::kLeft,
                                           MouseClickCount::kSingle))),
            "MouseClick[type=kLeft, count=kSingle]");
  EXPECT_EQ(DebugString(AsyncUiEvent(MouseClick(
                Handle(), MouseClickType::kRight, MouseClickCount::kDouble))),
            "MouseClick[type=kRight, count=kDouble]");
}

}  // namespace
}  // namespace actor::ui

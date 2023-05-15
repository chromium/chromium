// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/scoped_drag_drop_observer.h"

#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/draggable_test_view.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/test/ash_test_base.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Aliases.
using EventCallback = ScopedDragDropObserver::EventCallback;
using EventType = ScopedDragDropObserver::EventType;
using testing::AtLeast;
using testing::Conditional;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;

// ScopedDragDropObserverTest --------------------------------------------------

// Base class for tests of `ScopedDragDropObserver` parameterized by whether to
// test (a) `EventType::kDragCompleted` or (b) `EventType::kDragCancelled`.
class ScopedDragDropObserverTest
    : public AshTestBase,
      public testing::WithParamInterface</*test_scenario=*/EventType> {
 public:
  ScopedDragDropObserverTest() {
    // Sanity check parameterization.
    EXPECT_TRUE(GetTestScenario() == EventType::kDragCompleted ||
                GetTestScenario() == EventType::kDragCancelled);
  }

  // Returns (a) `EventType::kDragCompleted` or (b) `EventType::kDragCancelled`
  // depending on test parameterization.
  EventType GetTestScenario() const { return GetParam(); }

  // Returns a `ScopedDragDropObserver` with the specified `event_callback`.
  std::unique_ptr<ScopedDragDropObserver> CreateScopedDragDropObserver(
      EventCallback event_callback) {
    return std::make_unique<ScopedDragDropObserver>(
        aura::client::GetDragDropClient(
            widget_->GetNativeWindow()->GetRootWindow()),
        std::move(event_callback));
  }

  // Moves the mouse to the center of the specified `widget`.
  void MoveMouseTo(views::Widget* widget) {
    GetEventGenerator()->MoveMouseTo(
        widget->GetWindowBoundsInScreen().CenterPoint());
  }

  // Moves the mouse by the specified `x` and `y` offsets.
  void MoveMouseBy(int x, int y) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        event_generator->current_screen_location() + gfx::Vector2d(x, y),
        /*count=*/10);
  }

  // Presses/releases the left button.
  void PressLeftButton() { GetEventGenerator()->PressLeftButton(); }
  void ReleaseLeftButton() { GetEventGenerator()->ReleaseLeftButton(); }

  // Returns the `widget_` with draggable contents.
  views::Widget* widget() { return widget_.get(); }

 private:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Prevent blocking during drag-and-drop sequences.
    ShellTestApi().drag_drop_controller()->SetDisableNestedLoopForTesting(true);

    // Create and show a `widget_` with draggable contents.
    widget_ = CreateFramelessTestWidget();
    widget_->SetContentsView(std::make_unique<DraggableTestView>());
    widget_->CenterWindow(gfx::Size(100, 100));
    widget_->Show();
  }

  // Widget with draggable contents.
  std::unique_ptr<views::Widget> widget_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ScopedDragDropObserverTest,
                         /*test_scenario=*/
                         testing::Values(EventType::kDragCompleted,
                                         EventType::kDragCancelled));

// Tests -----------------------------------------------------------------------

// Verifies that `EventCallback`s are run as expected.
TEST_P(ScopedDragDropObserverTest, EventCallback) {
  base::MockCallback<EventCallback> event_callback;

  // Create observer.
  auto scoped_drag_drop_observer =
      CreateScopedDragDropObserver(event_callback.Get());

  {
    testing::InSequence sequence;

    // Expect one or more drag update events...
    EXPECT_CALL(event_callback,
                Run(Eq(EventType::kDragUpdated), /*event=*/NotNull()))
        .Times(AtLeast(1));

    // ...followed by a single drag completed or cancelled event.
    EXPECT_CALL(event_callback,
                Run(Eq(GetTestScenario()),
                    Conditional(GetTestScenario() == EventType::kDragCompleted,
                                /*event=*/NotNull(), /*event=*/IsNull())));
  }

  // Drag contents from `widget()`...
  MoveMouseTo(widget());
  PressLeftButton();
  MoveMouseBy(/*x=*/100, /*y=*/100);

  // ...then cancel drag...
  if (GetTestScenario() == EventType::kDragCancelled) {
    PressAndReleaseKey(ui::VKEY_ESCAPE);
  }

  // ...or complete drag.
  ReleaseLeftButton();
}

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/bubble_utils.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

// Creates a mouse event with a given event `target`.
ui::MouseEvent CreateEventWithTarget(aura::Window* target) {
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::Event::DispatcherApi(&event).set_target(target);
  return event;
}

using BubbleUtilsTest = AshTestBase;

TEST_F(BubbleUtilsTest, EventClosesBubble) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ui::MouseEvent event = CreateEventWithTarget(window.get());

  EXPECT_TRUE(bubble_utils::ShouldCloseBubbleForEvent(event));
}

TEST_F(BubbleUtilsTest, EventInCaptureModeDoesNotCloseBubble) {
  auto* controller = CaptureModeController::Get();
  controller->Start(CaptureModeEntryType::kAccelTakePartialScreenshot);

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ui::MouseEvent event = CreateEventWithTarget(window.get());

  EXPECT_FALSE(bubble_utils::ShouldCloseBubbleForEvent(event));
}

TEST_F(BubbleUtilsTest, EventInContainerDoesNotCloseBubble) {
  const int kTestCases[] = {kShellWindowId_MenuContainer,
                            kShellWindowId_VirtualKeyboardContainer,
                            kShellWindowId_SettingBubbleContainer};
  for (int container_id : kTestCases) {
    // Create a window and place it in the appropriate container.
    std::unique_ptr<aura::Window> window = CreateTestWindow();
    window->set_owned_by_parent(false);
    Shell::GetPrimaryRootWindowController()
        ->GetContainer(container_id)
        ->AddChild(window.get());

    ui::MouseEvent event = CreateEventWithTarget(window.get());
    EXPECT_FALSE(bubble_utils::ShouldCloseBubbleForEvent(event))
        << container_id;
  }
}

}  // namespace
}  // namespace ash

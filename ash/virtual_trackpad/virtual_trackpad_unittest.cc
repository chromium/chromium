// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_trackpad/virtual_trackpad_view.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

class VirtualTrackpadTest : public AshTestBase {
 public:
  VirtualTrackpadTest() = default;
  VirtualTrackpadTest(const VirtualTrackpadTest&) = delete;
  VirtualTrackpadTest& operator=(const VirtualTrackpadTest&) = delete;
  ~VirtualTrackpadTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshDebugShortcuts);
    AshTestBase::SetUp();
  }

  void ToggleVirtualTrackpad() {
    PressAndReleaseKey(ui::VKEY_Q, kDebugModifier);
  }

  views::Widget* GetWidget() {
    return VirtualTrackpadView::GetWidgetForTesting();
  }

  gfx::Rect GetVirtualTrackpadBoundsInScreen() {
    views::Widget* widget = GetWidget();
    DCHECK(widget);
    // Get contents view through the delegate because we set the contents view
    // through the delegate. The reasoning is explained in `Toggle()`.
    return static_cast<VirtualTrackpadView*>(
               widget->widget_delegate()->GetContentsView())
        ->GetTrackpadViewForTesting()
        ->GetBoundsInScreen();
  }

  void ClickFourFingerButton() {
    views::Widget* widget = GetWidget();
    DCHECK(widget);
    views::LabelButton* four_finger_button =
        static_cast<VirtualTrackpadView*>(
            widget->widget_delegate()->GetContentsView())
            ->finger_buttons_[4];

    LeftClickOn(four_finger_button);
  }
};

// Tests that the accelerator to show and hide the virtual trackpad widget works
// as expected.
TEST_F(VirtualTrackpadTest, ToggleShowHide) {
  ASSERT_FALSE(GetWidget());

  ToggleVirtualTrackpad();
  EXPECT_TRUE(GetWidget());

  ToggleVirtualTrackpad();
  // Toggle to close uses `views::Widget::Close()` which uses a post task.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetWidget());
}

// Tests that using the virtual trackpad allows us to enter and exit overview.
TEST_F(VirtualTrackpadTest, SwipeToToggleOverview) {
  ToggleVirtualTrackpad();

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Get the virtual trackpad bounds so we can use the mouse to generate scroll
  // events. Inset by a bit to ensure we don't click on the edges.
  gfx::Rect trackpad_bounds = GetVirtualTrackpadBoundsInScreen();
  trackpad_bounds.Inset(3);

  // Swipe up to enter overview. There should be no items in overview as the
  // virtual trackpad window is excluded from overview.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(trackpad_bounds.bottom_center());
  event_generator->DragMouseTo(trackpad_bounds.top_center());
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsEmpty());

  // Swipe down to exit overview.
  event_generator->DragMouseTo(trackpad_bounds.bottom_center());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

// Tests that using the virtual trackpad allows us to choose four fingers and
// swipe to an adjacent desk.
TEST_F(VirtualTrackpadTest, SwipeToSwitchDesks) {
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  ToggleVirtualTrackpad();

  // Get the virtual trackpad bounds so we can use the mouse to generate scroll
  // events. Inset by a bit to ensure we don't click on the edges.
  gfx::Rect trackpad_bounds = GetVirtualTrackpadBoundsInScreen();
  trackpad_bounds.Inset(3);

  // Try swiping across the virtual trackpad. The desk doesn't change as the
  // generated swipe is the default three fingers.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(trackpad_bounds.right_center());
  event_generator->DragMouseTo(trackpad_bounds.left_center());
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Click on the button which will make generated swipes be four fingers.
  // Swiping across the virtual trackpad will now change the desk.
  ClickFourFingerButton();
  event_generator->MoveMouseTo(trackpad_bounds.right_center());
  event_generator->DragMouseTo(trackpad_bounds.left_center());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
}

}  // namespace ash

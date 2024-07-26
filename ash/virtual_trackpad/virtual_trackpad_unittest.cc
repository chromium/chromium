// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/virtual_trackpad/virtual_trackpad_view.h"
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
    gfx::Rect bounds = static_cast<VirtualTrackpadView*>(
                           widget->widget_delegate()->GetContentsView())
                           ->GetTrackpadViewForTesting()
                           ->GetBoundsInScreen();

    // Inset by a bit to ensure we don't click on the edges.
    bounds.Inset(3);

    return bounds;
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

class VirtualTrackpadTestReverseScroll
    : public VirtualTrackpadTest,
      public testing::WithParamInterface<bool> {
 public:
  VirtualTrackpadTestReverseScroll() = default;
  VirtualTrackpadTestReverseScroll(const VirtualTrackpadTestReverseScroll&) =
      delete;
  VirtualTrackpadTestReverseScroll& operator=(
      const VirtualTrackpadTestReverseScroll&) = delete;
  ~VirtualTrackpadTestReverseScroll() override = default;

  void SetUp() override { VirtualTrackpadTest::SetUp(); }

  void SetNaturalScroll(bool enabled) {
    PrefService* pref =
        Shell::Get()->session_controller()->GetActivePrefService();
    pref->SetBoolean(prefs::kTouchpadEnabled, true);
    pref->SetBoolean(prefs::kNaturalScroll, enabled);
  }
};

// Tests that using the virtual trackpad allows us to enter and exit overview.
// Also tests this with reverse scrolling enabled.
TEST_P(VirtualTrackpadTestReverseScroll, SwipeToToggleOverview) {
  ToggleVirtualTrackpad();

  // Toggle natural scrolling. Behavior should stay the same.
  SetNaturalScroll(/*enabled=*/GetParam());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  ASSERT_FALSE(overview_controller->InOverviewSession());

  // Get the trackpad bounds so we can use the mouse to generate scroll events.
  gfx::Rect trackpad_bounds = GetVirtualTrackpadBoundsInScreen();

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
// swipe to an adjacent desk. Also tests this with reverse scrolling enabled.
TEST_P(VirtualTrackpadTestReverseScroll, SwipeToSwitchDesks) {
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  desks_controller->ActivateDesk(desks_controller->GetDeskAtIndex(1),
                                 DesksSwitchSource::kUserSwitch);
  ASSERT_EQ(1, desks_controller->GetActiveDeskIndex());

  ToggleVirtualTrackpad();

  // Toggle natural scrolling.
  SetNaturalScroll(/*enabled=*/GetParam());

  // Get the trackpad bounds so we can use the mouse to generate scroll events.
  gfx::Rect trackpad_bounds = GetVirtualTrackpadBoundsInScreen();

  // Try swiping across the virtual trackpad. The desk doesn't change as the
  // generated swipe is the default three fingers.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(trackpad_bounds.right_center());
  event_generator->DragMouseTo(trackpad_bounds.left_center());
  ASSERT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Click on the button which will make generated swipes be four fingers.
  // Swiping across the virtual trackpad will now change the desk.
  ClickFourFingerButton();
  event_generator->MoveMouseTo(trackpad_bounds.right_center());
  event_generator->DragMouseTo(trackpad_bounds.left_center());

  // Natural scroll should flip the behavior of scrolls.
  bool natural_scroll_enabled = GetParam();
  EXPECT_EQ(natural_scroll_enabled ? 2 : 0,
            desks_controller->GetActiveDeskIndex());
}

INSTANTIATE_TEST_SUITE_P(All,
                         VirtualTrackpadTestReverseScroll,
                         ::testing::Bool());

}  // namespace ash

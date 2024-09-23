// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

constexpr int kPanelWidth = 200;
constexpr int kPanelHeight = 300;
constexpr gfx::Rect kInitialBounds(100, 100, kPanelWidth, kPanelHeight);

class MahiPanelDragBehaviorTest : public AshTestBase {
 public:
  MahiPanelDragBehaviorTest() {
    ON_CALL(mock_mahi_manager_, IsEnabled).WillByDefault(Return(true));
  }
  ~MahiPanelDragBehaviorTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});
    AshTestBase::SetUp();
    ui_controller_.OpenMahiPanel(GetPrimaryDisplay().id(),
                                 /*mahi_menu_bounds=*/gfx::Rect());
  }

  void TearDown() override {
    ui_controller_.CloseMahiPanel();
    AshTestBase::TearDown();
  }

  MahiUiController& ui_controller() { return ui_controller_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  MahiUiController ui_controller_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  chromeos::ScopedMahiManagerSetter scoped_manager_setter_{&mock_mahi_manager_};
};

TEST_F(MahiPanelDragBehaviorTest, MouseDragRepositionsPanel) {
  views::Widget* panel_widget = ui_controller().mahi_panel_widget();
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(20, 10);
  GetEventGenerator()->DragMouseBy(kDragOffset.x(), kDragOffset.y());

  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
}

TEST_F(MahiPanelDragBehaviorTest, GestureDragRepositionsPanel) {
  views::Widget* panel_widget = ui_controller().mahi_panel_widget();
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(20, 10);
  GetEventGenerator()->PressMoveAndReleaseTouchBy(kDragOffset.x(),
                                                  kDragOffset.y());

  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
}

TEST_F(MahiPanelDragBehaviorTest, MouseDragOutOfTheScreenEdge) {
  UpdateDisplay("1000x500");
  views::Widget* panel_widget = ui_controller().mahi_panel_widget();
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(1100, 600);
  GetEventGenerator()->DragMouseBy(kDragOffset.x(), kDragOffset.y());

  // If the panel is dragged out of the edge, it should show 1/3 of its height
  // and width on the screen.
  constexpr gfx::Rect kMaxBounds(1000 /*display width*/ - kPanelWidth / 3.0,
                                 500 /*display height*/ - kPanelHeight / 3.0,
                                 kPanelWidth, kPanelHeight);
  EXPECT_NE(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(), kMaxBounds);
}

TEST_F(MahiPanelDragBehaviorTest, MouseDragInMultiScreens) {
  // Adds 3 displays.
  UpdateDisplay("1000x500,1000x500,1000x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Puts the second display on the right of the first one and the third display
  // on the bottom of the second one.
  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(), list[0]);
  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0],
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[1],
                              display::DisplayPlacement::BOTTOM, 0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  views::Widget* panel_widget = ui_controller().mahi_panel_widget();
  panel_widget->SetBounds(kInitialBounds);

  // The panel is on the first display.
  EXPECT_EQ(panel_widget->GetNativeWindow()->GetRootWindow(), root_windows[0]);

  // Drags the panel from the first screen to the second screen.
  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset1(1500, 50);
  GetEventGenerator()->DragMouseBy(kDragOffset1.x(), kDragOffset1.y());

  // The panel is on the second display now.
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset1);
  EXPECT_EQ(panel_widget->GetNativeWindow()->GetRootWindow(), root_windows[1]);

  // Drags the panel from the second screen to the third screen.
  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset2(50, 600);
  GetEventGenerator()->DragMouseBy(kDragOffset2.x(), kDragOffset2.y());

  // The panel is on the third display now.
  EXPECT_EQ(panel_widget->GetNativeWindow()->GetRootWindow(), root_windows[2]);
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset1 + kDragOffset2);
}

}  // namespace

}  // namespace ash

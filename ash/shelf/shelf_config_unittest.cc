// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_config.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShelfConfigTest : public AshTestBase {
 public:
  ShelfConfigTest() = default;
  ~ShelfConfigTest() override = default;

 protected:
  bool is_dense() { return ShelfConfig::Get()->is_dense_; }

  bool IsTabletMode() { return display::Screen::GetScreen()->InTabletMode(); }

  void SetTabletMode(bool is_tablet_mode) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(is_tablet_mode);
  }
};

// Make sure ShelfConfig is dense when screen becomes small in tablet mode.
TEST_F(ShelfConfigTest, SmallDisplayIsDense) {
  UpdateDisplay("1100x1000");
  SetTabletMode(true);

  ASSERT_TRUE(IsTabletMode());
  ASSERT_FALSE(is_dense());

  // Change display to have a small width, and check that ShelfConfig is dense.
  UpdateDisplay("300x1000");
  ASSERT_TRUE(is_dense());

  // Set the display size back.
  UpdateDisplay("1100x1000");
  ASSERT_FALSE(is_dense());

  // Change display to have a small height, and check that ShelfConfig is dense.
  UpdateDisplay("1100x300");
  ASSERT_TRUE(is_dense());
}

// Make sure ShelfConfig switches between dense and not dense when switching
// between clamshell and tablet mode.
TEST_F(ShelfConfigTest, DenseChangeOnTabletModeChange) {
  UpdateDisplay("1100x1000");

  ASSERT_FALSE(IsTabletMode());
  ASSERT_TRUE(is_dense());

  SetTabletMode(true);

  ASSERT_TRUE(IsTabletMode());
  ASSERT_FALSE(is_dense());

  SetTabletMode(false);

  ASSERT_FALSE(IsTabletMode());
  ASSERT_TRUE(is_dense());
}

// Make sure that the shelf size changes when switching between contexts:
// home vs in-app, clamshell vs tablet, dense vs not dense.
TEST_F(ShelfConfigTest, ShelfSizeChangesWithContext) {
  // For shelf size, the ordering should be as follows, in increasing order:
  // * Tablet, dense, in-app
  // * Tablet, standard, in-app
  // * Tablet, dense, home == Clamshell (all modes)
  // * Tablet, standard, home

  // For shelf control button size, the ordering should be as follows, in
  // increasing order:
  // * Tablet, dense (in-app and home) == Clamshell (all modes)
  // * Tablet, standard (in-app and home)

  UpdateDisplay("300x1000");
  SetTabletMode(true);
  ASSERT_TRUE(IsTabletMode());
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  GetAppListTestHelper()->CheckVisibility(false);
  const int tablet_dense_in_app = ShelfConfig::Get()->shelf_size();
  const int system_shelf_tablet_dense_in_app =
      ShelfConfig::Get()->system_shelf_size();
  const int control_tablet_dense_in_app = ShelfConfig::Get()->control_size();

  widget->Close();
  GetAppListTestHelper()->CheckVisibility(true);
  const int tablet_dense_home = ShelfConfig::Get()->shelf_size();
  const int system_shelf_tablet_dense_home =
      ShelfConfig::Get()->system_shelf_size();
  const int control_tablet_dense_home = ShelfConfig::Get()->control_size();

  UpdateDisplay("1100x1000");
  ASSERT_TRUE(IsTabletMode());
  widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  GetAppListTestHelper()->CheckVisibility(false);
  const int tablet_standard_in_app = ShelfConfig::Get()->shelf_size();
  const int system_shelf_tablet_standard_in_app =
      ShelfConfig::Get()->system_shelf_size();
  const int control_tablet_standard_in_app = ShelfConfig::Get()->control_size();

  widget->Close();
  GetAppListTestHelper()->CheckVisibility(true);
  const int tablet_standard_home = ShelfConfig::Get()->shelf_size();
  const int system_shelf_tablet_standard_home =
      ShelfConfig::Get()->system_shelf_size();
  const int control_tablet_standard_home = ShelfConfig::Get()->control_size();

  SetTabletMode(false);
  ASSERT_FALSE(IsTabletMode());
  GetAppListTestHelper()->Dismiss();
  const int clamshell_home = ShelfConfig::Get()->shelf_size();
  const int system_shelf_clamshell_home =
      ShelfConfig::Get()->system_shelf_size();
  const int control_clamshell_home = ShelfConfig::Get()->control_size();

  widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->Maximize();
  const int clamshell_in_app = ShelfConfig::Get()->shelf_size();
  const int system_shelf_clamshell_in_app =
      ShelfConfig::Get()->system_shelf_size();
  const int control_clamshell_in_app = ShelfConfig::Get()->control_size();

  EXPECT_LT(tablet_dense_in_app, tablet_standard_in_app);
  EXPECT_LT(tablet_standard_in_app, tablet_dense_home);
  EXPECT_EQ(tablet_dense_home, clamshell_home);
  EXPECT_EQ(clamshell_home, clamshell_in_app);
  EXPECT_LT(tablet_dense_home, tablet_standard_home);

  EXPECT_EQ(control_tablet_dense_in_app, control_tablet_dense_home);
  EXPECT_EQ(control_tablet_dense_home, control_clamshell_in_app);
  EXPECT_EQ(control_clamshell_in_app, control_clamshell_home);
  EXPECT_LT(control_clamshell_home, control_tablet_standard_in_app);
  EXPECT_EQ(control_tablet_standard_in_app, control_tablet_standard_home);

  // System shelf size should return size that matches out-of-app (home) state.
  EXPECT_EQ(system_shelf_tablet_dense_in_app, tablet_dense_home);
  EXPECT_EQ(system_shelf_tablet_dense_home, tablet_dense_home);
  EXPECT_EQ(system_shelf_tablet_standard_in_app, tablet_standard_home);
  EXPECT_EQ(system_shelf_tablet_standard_home, tablet_standard_home);
  EXPECT_EQ(system_shelf_clamshell_home, clamshell_home);
  EXPECT_EQ(system_shelf_clamshell_in_app, clamshell_home);
}

// Make sure that we consider ourselves inside an app when appropriate.
TEST_F(ShelfConfigTest, InAppMode) {
  SessionInfo info;
  info.state = session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);

  // Go into tablet mode, open a window. Now we're in an app.
  SetTabletMode(true);
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());

  // Close the window. We should be back on the home screen.
  widget->Close();
  EXPECT_FALSE(ShelfConfig::Get()->is_in_app());

  // Open a window again.
  widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());

  // Now go into overview.
  EnterOverview();
  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());

  // Back to the app.
  ExitOverview();
  EXPECT_TRUE(ShelfConfig::Get()->is_in_app());

  // Leave the session.
  info.state = session_manager::SessionState::LOGIN_PRIMARY;
  Shell::Get()->session_controller()->SetSessionInfo(info);
  EXPECT_FALSE(ShelfConfig::Get()->is_in_app());
}

}  // namespace ash

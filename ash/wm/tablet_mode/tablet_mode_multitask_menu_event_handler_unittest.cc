// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/wm/features.h"

namespace ash {

class TabletModeMultitaskMenuEventHandlerTest : public AshTestBase {
 public:
  TabletModeMultitaskMenuEventHandlerTest() = default;
  TabletModeMultitaskMenuEventHandlerTest(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  TabletModeMultitaskMenuEventHandlerTest& operator=(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  ~TabletModeMultitaskMenuEventHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kFloatWindow);

    AshTestBase::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that a swipe down gesture from the top center activates the multitask
// menu.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, SwipeDown) {
  auto window = CreateTestWindow();

  // Start the swipe from the top center of the window.
  const int point_x = window->bounds().CenterPoint().x();
  GetEventGenerator()->GestureScrollSequence(gfx::Point(point_x, 1),
                                             gfx::Point(point_x, 10),
                                             base::Milliseconds(100), 3);

  TabletModeWindowManager* manager =
      TabletModeControllerTestApi().tablet_mode_window_manager();
  ASSERT_TRUE(manager);

  auto* event_handler =
      manager->tablet_mode_multitask_menu_event_handler_for_testing();
  ASSERT_TRUE(event_handler);

  auto* multitask_menu = event_handler->multitask_menu_for_testing();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(multitask_menu->multitask_menu_widget_for_testing()
                  ->GetContentsView()
                  ->GetVisible());
}

}  // namespace ash
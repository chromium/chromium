// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_unified.h"

#include "ash/display/mirror_window_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/ozone/chromeos/cursor_controller.h"

namespace ash {

namespace {

class AshWindowTreeHostUnifiedTest : public AshTestBase {
 public:
  AshWindowTreeHostUnifiedTest() = default;
  AshWindowTreeHostUnifiedTest(const AshWindowTreeHostUnifiedTest&) = delete;
  AshWindowTreeHostUnifiedTest& operator=(const AshWindowTreeHostUnifiedTest&) =
      delete;

  void SetUp() override {
    AshTestBase::SetUp();
    display_manager()->SetUnifiedDesktopEnabled(true);
  }
};

}  // namespace

TEST_F(AshWindowTreeHostUnifiedTest, RotatedMouseMovement) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay("1920x1080*2,800x600/r");

  MirrorWindowTestApi test_api;
  std::vector<aura::WindowTreeHost*> hosts = test_api.GetHosts();
  // Have 2 WindowTreeHosts, one per display.
  ASSERT_EQ(2u, hosts.size());

  gfx::Vector2dF move;
  ui::CursorController* controller = ui::CursorController::GetInstance();

  gfx::AcceleratedWidget widget0 = hosts[0]->GetAcceleratedWidget();
  move = {2, 0};
  controller->ApplyCursorConfigForWindow(widget0, &move);
  EXPECT_EQ(gfx::Vector2dF(2, 0), move);
  move = {0, 2};
  controller->ApplyCursorConfigForWindow(widget0, &move);
  EXPECT_EQ(gfx::Vector2dF(0, 2), move);

  gfx::AcceleratedWidget widget1 = hosts[1]->GetAcceleratedWidget();
  move = {2, 0};
  controller->ApplyCursorConfigForWindow(widget1, &move);
  // There's a 1.2 scaling for external displays.
  EXPECT_EQ(gfx::Vector2dF(0, 2.4), move);
  move = {0, 2};
  controller->ApplyCursorConfigForWindow(widget1, &move);
  EXPECT_EQ(gfx::Vector2dF(-2.4, 0), move);

  // Verify the old config is removed when display is updated.
  gfx::AcceleratedWidget old_widget1 = widget1;

  // TODO(crbug.com/40215149): Just updating the display rotation doesn't update
  // the cursor config, so need to remove and re-add the display for it to
  // re-register a new cursor config. After the bug is fixed, change to just
  // update to the new rotation directly. In that case, the widget id may stay
  // the same, so update the test accordingly.
  UpdateDisplay("1920x1080*2");
  UpdateDisplay("1920x1080*2,800x600/l");

  hosts = test_api.GetHosts();
  gfx::AcceleratedWidget new_widget1 = hosts[1]->GetAcceleratedWidget();
  EXPECT_NE(new_widget1, old_widget1);

  move = {2, 0};
  controller->ApplyCursorConfigForWindow(new_widget1, &move);
  EXPECT_EQ(gfx::Vector2dF(0, -2.4), move);

  // Old widget should have no transformation.
  move = {2, 0};
  controller->ApplyCursorConfigForWindow(old_widget1, &move);
  EXPECT_EQ(gfx::Vector2dF(2, 0), move);
}

}  // namespace ash

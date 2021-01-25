// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/full_restore/full_restore_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

namespace {

void PerformAcceleratorAction(AcceleratorAction action,
                              const ui::Accelerator& accelerator) {
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(action,
                                                                 accelerator);
}

}  // namespace

class FullRestoreControllerTest : public AshTestBase {
 public:
  FullRestoreControllerTest() = default;
  FullRestoreControllerTest(const FullRestoreControllerTest&) = delete;
  FullRestoreControllerTest& operator=(const FullRestoreControllerTest&) =
      delete;
  ~FullRestoreControllerTest() override = default;

  int GetSaveWindowsCount() const {
    return FullRestoreController::Get()->save_windows_count_for_testing_;
  }

  void ResetSaveWindowsCount() {
    FullRestoreController::Get()->save_windows_count_for_testing_ = 0;
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kFullRestore);
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that data gets saved when an application window is created and the data
// gets removed when the application is closed.
TEST_F(FullRestoreControllerTest, AppWindowAddedClosed) {
  const gfx::Rect bounds(200, 200);
  auto browser_window = CreateAppWindow(bounds, AppType::BROWSER);
  auto chrome_app_window = CreateAppWindow(bounds, AppType::CHROME_APP);

  // For now, creating a window will trigger two saves, one when adding the
  // window to its parent and one when the window gets its initial bounds set.
  // The actual writing to the database is throttled, so this is ok.
  EXPECT_EQ(4, GetSaveWindowsCount());

  // Tests we save each time a viable app window is destroyed.
  browser_window.reset();
  chrome_app_window.reset();
  EXPECT_EQ(6, GetSaveWindowsCount());

  // Test that creating and destroying a system app writes nothing to the
  // database.
  // TODO(crbug.com/1164472): Checking app type is temporary solution until we
  // can get windows which are allowed to full restore from the
  // FullRestoreService.
  ResetSaveWindowsCount();
  auto system_window = CreateAppWindow(bounds, AppType::SYSTEM_APP);
  system_window.reset();
  EXPECT_EQ(0, GetSaveWindowsCount());
}

// Tests that data gets saved when changing a window's window state.
TEST_F(FullRestoreControllerTest, WindowStateChanged) {
  auto window = CreateAppWindow(gfx::Rect(600, 600), AppType::BROWSER);
  ResetSaveWindowsCount();

  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  EXPECT_EQ(1, GetSaveWindowsCount());

  window_state->Unminimize();
  EXPECT_EQ(2, GetSaveWindowsCount());

  // Maximize and restore will invoke two calls to SaveWindows because
  // their animations also change the bounds of the window. The actual writing
  // to the database is throttled, so this is ok.
  window_state->Activate();
  window_state->Maximize();
  EXPECT_EQ(4, GetSaveWindowsCount());

  window_state->Restore();
  EXPECT_EQ(6, GetSaveWindowsCount());

  PerformAcceleratorAction(WINDOW_CYCLE_SNAP_LEFT, {});
  EXPECT_EQ(7, GetSaveWindowsCount());
}

// Tests that data gets saved when moving a window to another desk.
TEST_F(FullRestoreControllerTest, WindowMovedDesks) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(0, desks_controller->GetDeskIndex(
                   desks_controller->GetTargetActiveDesk()));

  auto window = CreateAppWindow(gfx::Rect(100, 100), AppType::BROWSER);
  aura::Window* previous_parent = window->parent();
  ResetSaveWindowsCount();

  // Move the window to the desk on the right. Test that we save the window in
  // the database.
  PerformAcceleratorAction(
      DESKS_MOVE_ACTIVE_ITEM,
      {ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN});
  ASSERT_NE(previous_parent, window->parent());
  EXPECT_EQ(1, GetSaveWindowsCount());
}

// Tests that data gets saved when moving a window to another display using the
// accelerator.
TEST_F(FullRestoreControllerTest, WindowMovedDisplay) {
  UpdateDisplay("800x800,801+0-800x800");

  auto window = CreateAppWindow(gfx::Rect(50, 50, 100, 100), AppType::BROWSER);
  ResetSaveWindowsCount();

  // Move the window to the next display. Test that we save the window in
  // the database.
  PerformAcceleratorAction(MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS, {});
  ASSERT_TRUE(
      gfx::Rect(801, 0, 800, 800).Contains(window->GetBoundsInScreen()));
  EXPECT_EQ(1, GetSaveWindowsCount());
}

// Tests that data gets saved when dragging a window.
TEST_F(FullRestoreControllerTest, WindowDragged) {
  auto window = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  ResetSaveWindowsCount();

  const gfx::Point point_on_frame(200, 16);
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(point_on_frame);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(15, 15);
  event_generator->MoveMouseBy(15, 15);
  event_generator->MoveMouseBy(15, 15);
  event_generator->ReleaseLeftButton();

  EXPECT_EQ(3, GetSaveWindowsCount());
}

TEST_F(FullRestoreControllerTest, TabletModeChange) {
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(1, GetSaveWindowsCount());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(2, GetSaveWindowsCount());
}

TEST_F(FullRestoreControllerTest, DisplayAddRemove) {
  UpdateDisplay("800x800,801+0-800x800");

  auto window = CreateAppWindow(gfx::Rect(800, 0, 400, 400), AppType::BROWSER);
  ResetSaveWindowsCount();

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t second_id = display_manager()->GetDisplayAt(1).id();
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo second_info =
      display_manager()->GetDisplayInfo(second_id);

  // Remove the secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1, GetSaveWindowsCount());

  // Reconnect the secondary display. PersistentWindowController will move the
  // window back to the secondary display, so a save should be triggered.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2, GetSaveWindowsCount());
}

}  // namespace ash

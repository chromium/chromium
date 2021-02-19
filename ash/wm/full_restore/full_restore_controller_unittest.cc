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
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_util.h"

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
  // Struct which is the data in our fake full restore file.
  struct WindowInfo {
    int call_count = 0;
    int activation_index = 0;
  };

  FullRestoreControllerTest() = default;
  FullRestoreControllerTest(const FullRestoreControllerTest&) = delete;
  FullRestoreControllerTest& operator=(const FullRestoreControllerTest&) =
      delete;
  ~FullRestoreControllerTest() override = default;

  // Returns the number of times |window| has been saved to file since the last
  // ResetSaveWindowsCount call.
  int GetSaveWindowsCount(aura::Window* window) const {
    if (!base::Contains(fake_full_restore_file_, window))
      return 0;
    return fake_full_restore_file_.at(window).call_count;
  }

  // Returns the total number of saves since the last ResetSaveWindowsCount
  // call.
  int GetTotalSaveWindowsCount() const {
    int count = 0;
    for (const std::pair<aura::Window*, WindowInfo>& member :
         fake_full_restore_file_) {
      count += member.second.call_count;
    }
    return count;
  }

  void ResetSaveWindowsCount() {
    for (std::pair<aura::Window*, WindowInfo>& member : fake_full_restore_file_)
      member.second.call_count = 0;
  }

  // Returns the stored activation index for |window|.
  int GetActivationIndex(aura::Window* window) const {
    if (!base::Contains(fake_full_restore_file_, window))
      return -1;
    return fake_full_restore_file_.at(window).activation_index;
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kFullRestore);

    AshTestBase::SetUp();

    FullRestoreController::Get()->SetSaveWindowCallbackForTesting(
        base::BindRepeating(&FullRestoreControllerTest::OnSaveWindow,
                            base::Unretained(this)));
  }

 private:
  // Called when FullRestoreController saves a window to the file. Immediately
  // writes to our fake file |fake_full_restore_file_|.
  void OnSaveWindow(const full_restore::WindowInfo& window_info) {
    aura::Window* window = window_info.window;
    DCHECK(window_info.activation_index);

    if (fake_full_restore_file_.contains(window)) {
      fake_full_restore_file_[window].call_count++;
      fake_full_restore_file_[window].activation_index =
          *window_info.activation_index;
    } else {
      fake_full_restore_file_[window] = {0, *window_info.activation_index};
    }
  }

  // A map which is a fake representation of the full restore file.
  base::flat_map<aura::Window*, WindowInfo> fake_full_restore_file_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that data gets saved when changing a window's window state.
TEST_F(FullRestoreControllerTest, WindowStateChanged) {
  auto window = CreateAppWindow(gfx::Rect(600, 600), AppType::BROWSER);
  ResetSaveWindowsCount();

  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));

  window_state->Unminimize();
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));

  window_state->Activate();
  EXPECT_EQ(3, GetSaveWindowsCount(window.get()));

  // Maximize and restore will invoke two calls to SaveWindows because
  // their animations also change the bounds of the window. The actual writing
  // to the database is throttled, so this is ok.
  window_state->Maximize();
  EXPECT_EQ(5, GetSaveWindowsCount(window.get()));

  window_state->Restore();
  EXPECT_EQ(7, GetSaveWindowsCount(window.get()));

  PerformAcceleratorAction(WINDOW_CYCLE_SNAP_LEFT, {});
  EXPECT_EQ(8, GetSaveWindowsCount(window.get()));
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
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when assigning a window to all desks.
TEST_F(FullRestoreControllerTest, AssignToAllDesks) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(0, desks_controller->GetDeskIndex(
                   desks_controller->GetTargetActiveDesk()));

  auto window = CreateAppWindow(gfx::Rect(100, 100), AppType::BROWSER);
  ResetSaveWindowsCount();

  // Assign |window| to all desks. This should trigger a save.
  window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey, true);
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));

  // Unassign |window| from all desks. This should trigger a save.
  window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey, false);
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));
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
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when dragging a window.
TEST_F(FullRestoreControllerTest, WindowDragged) {
  auto window = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  ResetSaveWindowsCount();

  // Test that even if we move n times, we will only save to file once.
  const gfx::Point point_on_frame(200, 16);
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(point_on_frame);
  event_generator->PressLeftButton();
  for (int i = 0; i < 5; ++i)
    event_generator->MoveMouseBy(15, 15);
  event_generator->ReleaseLeftButton();

  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

TEST_F(FullRestoreControllerTest, TabletModeChange) {
  // Tests that with no windows, nothing gets save when entering or exiting
  // tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(0, GetTotalSaveWindowsCount());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(0, GetTotalSaveWindowsCount());

  auto window1 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  auto window2 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  ResetSaveWindowsCount();

  // Tests that we save each window when entering or exiting tablet mode. Due to
  // many possible things changing during a tablet switch (window state, bounds,
  // etc.), we cannot determine exactly how many saves there will be, but there
  // should be more than one per window.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_GT(GetSaveWindowsCount(window1.get()), 1);
  EXPECT_GT(GetSaveWindowsCount(window2.get()), 1);

  ResetSaveWindowsCount();
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_GT(GetSaveWindowsCount(window1.get()), 1);
  EXPECT_GT(GetSaveWindowsCount(window2.get()), 1);
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

  // Remove the secondary display. Doing so will change both the bounds of the
  // window and activate it, resulting in a double save.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));

  // Reconnect the secondary display. PersistentWindowController will move the
  // window back to the secondary display, so a save should be triggered.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(3, GetSaveWindowsCount(window.get()));
}

TEST_F(FullRestoreControllerTest, Activation) {
  auto window1 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  auto window2 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  auto window3 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  ResetSaveWindowsCount();

  // Tests that an activation will save once for each window.
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(3, GetTotalSaveWindowsCount());

  // Tests that most recently used windows have the highest activation index.
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());
  EXPECT_EQ(0, GetActivationIndex(window1.get()));
  EXPECT_EQ(1, GetActivationIndex(window2.get()));
  EXPECT_EQ(2, GetActivationIndex(window3.get()));
}

}  // namespace ash

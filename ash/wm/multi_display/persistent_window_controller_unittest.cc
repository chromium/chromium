// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multi_display/persistent_window_controller.h"
#include "ash/display/display_move_window_util.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/screen_util.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/display/test/display_manager_test_api.h"

using session_manager::SessionState;

namespace ash {

using PersistentWindowControllerTest = AshTestBase;

display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                              const gfx::Rect& bounds) {
  display::ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);
  // Each display should have at least one native mode.
  display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                   /*is_interlaced=*/true,
                                   /*native=*/true);
  info.SetManagedDisplayModes({mode});
  return info;
}

TEST_F(PersistentWindowControllerTest, DisconnectDisplay) {
  UpdateDisplay("500x600,500x600");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects primary display.
  display_info_list.clear();
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects primary display.
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects secondary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // A third id which is different from primary and secondary.
  const int64_t third_id = secondary_id + 1;
  display::ManagedDisplayInfo third_info =
      CreateDisplayInfo(third_id, gfx::Rect(0, 501, 600, 500));
  // Connects another secondary display with |third_id|.
  display_info_list.push_back(third_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  // Connects secondary display with |secondary_id|.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects secondary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Sets |w2|'s bounds changed by user and then reconnects secondary display.
  WindowState* w2_state = WindowState::Get(w2);
  w2_state->SetBoundsChangedByUser(true);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, ThreeDisplays) {
  UpdateDisplay("500x600,500x600,500x600");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  aura::Window* w3 =
      CreateTestWindowInShellWithBounds(gfx::Rect(1002, 0, 400, 200));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());

  const int64_t primary_id = display_manager()->GetDisplayAt(0).id();
  const int64_t second_id = display_manager()->GetDisplayAt(1).id();
  const int64_t third_id = display_manager()->GetDisplayAt(2).id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo second_info =
      display_manager()->GetDisplayInfo(second_id);
  display::ManagedDisplayInfo third_info =
      display_manager()->GetDisplayInfo(third_id);

  // Disconnects third display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Disconnects second display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects third display.
  display_info_list.push_back(third_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(502, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects second display.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());

  // Disconnects both external displays.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects second display.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects third display.
  display_info_list.push_back(third_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, NormalMirrorMode) {
  UpdateDisplay("500x600,500x600");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Enables mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  // Disables mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
}

// Tests that mirror and un-mirror a display with non-identical scale factor
// (not 1.0f).
TEST_F(PersistentWindowControllerTest,
       MirrorDisplayWithNonIdenticalScaleFactor) {
  UpdateDisplay("500x600,500x600*1.2");
  ASSERT_EQ(1.2f, display_manager()->GetDisplayAt(1).device_scale_factor());

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));

  // Enables mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Disables mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  // The window should still be restored to the display with non-identical scale
  // factor.
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1.2f, display_manager()->GetDisplayAt(1).device_scale_factor());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, MixedMirrorMode) {
  UpdateDisplay("500x600,500x600,500x600");
  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  aura::Window* w3 =
      CreateTestWindowInShellWithBounds(gfx::Rect(1002, 0, 400, 200));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());

  const int64_t primary_id = display_manager()->GetDisplayAt(0).id();
  const int64_t second_id = display_manager()->GetDisplayAt(1).id();
  const int64_t third_id = display_manager()->GetDisplayAt(2).id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo second_info =
      display_manager()->GetDisplayInfo(second_id);
  display::ManagedDisplayInfo third_info =
      display_manager()->GetDisplayInfo(third_id);

  // Turn on mixed mirror mode. (Mirror from the primary display to the second
  // display).
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(second_id);
  display_manager()->SetMirrorMode(
      display::MirrorMode::kMixed,
      std::make_optional<display::MixedMirrorModeParams>(primary_id, dst_ids));
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(502, 0, 400, 200), w3->GetBoundsInScreen());

  // Turn off mixed mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, WindowMovedByAccel) {
  UpdateDisplay("500x600,500x600");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Moves |w2| to primary display by accelerators after we reset the persistent
  // window info. It should be able to save persistent window info again on next
  // display change.
  wm::ActivateWindow(w2);
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects secondary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, ReconnectOnLockScreen) {
  UpdateDisplay("500x600,500x600");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Enters locked session state and reconnects secondary display.
  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Unlocks and checks that |w2| is restored.
  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, RecordNumOfWindowsRestored) {
  UpdateDisplay("500x600,500x600");
  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      PersistentWindowController::kNumOfWindowsRestoredOnDisplayAdded, 0);

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  histogram_tester.ExpectTotalCount(
      PersistentWindowController::kNumOfWindowsRestoredOnDisplayAdded, 1);
}

// Tests that swapping primary display shall not do persistent window restore.
TEST_F(PersistentWindowControllerTest, SwapPrimaryDisplay) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 600));
  const display::ManagedDisplayInfo secondary_display_info =
      CreateDisplayInfo(10, gfx::Rect(1, 1, 400, 500));

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_info_list.push_back(secondary_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Swaps primary display and check window bounds.
  SwapPrimaryDisplay();
  ASSERT_EQ(gfx::Rect(-500, 0, 500, 600),
            display_manager()->GetDisplayForId(internal_display_id).bounds());
  ASSERT_EQ(gfx::Rect(0, 0, 400, 500),
            display_manager()->GetDisplayForId(10).bounds());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(-499, 0, 200, 100), w2->GetBoundsInScreen());
}

// Tests that restore bounds persist after adding and removing a display.
TEST_F(PersistentWindowControllerTest, RestoreBounds) {
  UpdateDisplay("500x600,500x600");

  std::unique_ptr<aura::Window> window = CreateTestWindow(gfx::Rect(200, 200));
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(primary_id, screen->GetDisplayNearestWindow(window.get()).id());

  // Move the window to the secondary display and maximize it.
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  EXPECT_TRUE(window_state->HasRestoreBounds());
  const gfx::Rect restore_bounds_in_screen =
      window_state->GetRestoreBoundsInScreen();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnect secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(window.get()).id());

  // Reconnect secondary display. On restoring the maximized window, the bounds
  // should be the same as they were before maximizing and disconnecting the
  // display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsMaximized());

  // Restore the window (i.e. press restore button on header).
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(restore_bounds_in_screen, window->GetBoundsInScreen());
}

// Tests that restore bounds updated correctly after removing and adding back
// the internal display.
TEST_F(PersistentWindowControllerTest, RestoreBoundsOnInternalDisplayRemoval) {
  UpdateDisplay("500x600,500x700");

  std::unique_ptr<aura::Window> window = CreateTestWindow(gfx::Rect(200, 100));
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(primary_id, screen->GetDisplayNearestWindow(window.get()).id());

  // Move the window to the secondary display and snap it.
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  WindowState* window_state = WindowState::Get(window.get());
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_TRUE(window_state->HasRestoreBounds());
  const gfx::Rect restore_bounds_in_screen =
      window_state->GetRestoreBoundsInScreen();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnect the primary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  // TODO(b/291341473): The restore bounds of the window should be updated
  // correctly on the display changes.

  // Reconnect the primary display.
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  // The window should still stay in the secondary display with resumed restore
  // bounds.
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(restore_bounds_in_screen, window_state->GetRestoreBoundsInScreen());

  // Maximize the window, it should stay in the secondary display.
  window_state->Maximize();
  ASSERT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());

  // Restore the window, it should go back to snapped state and stay in the
  // secondary display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());

  // Restore again, the window should go back to normal state and stay in the
  // secondary display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
}

// Tests that the MRU order is maintained visually after adding and removing a
// display.
TEST_F(PersistentWindowControllerTest, MRUOrderMatchesStacking) {
  UpdateDisplay("500x600,500x600");

  // Add three windows, all on the secondary display.
  const gfx::Rect bounds(500, 0, 200, 200);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);

  // MRU order should be opposite of the order the windows were created. Verify
  // that all three windows are indeed on the secondary display.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<raw_ptr<aura::Window, VectorExperimental>>
      expected_mru_order = {window3.get(), window2.get(), window1.get()};
  ASSERT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  for (aura::Window* window : expected_mru_order) {
    ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(window).id());
  }

  // Disconnect secondary display. The windows should move to the primary
  // display and retain MRU ordering.
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The order which the children are stacked in is the reverse of the order
  // they are in the children() field.
  aura::Window* parent = window1->parent();
  ASSERT_TRUE(parent);
  std::vector<raw_ptr<aura::Window, VectorExperimental>>
      children_ordered_by_stacking = parent->children();
  std::reverse(children_ordered_by_stacking.begin(),
               children_ordered_by_stacking.end());
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  EXPECT_EQ(expected_mru_order, children_ordered_by_stacking);
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(parent).id());

  // Reconnect secondary display. The windows should move to the secondary
  // display and retain MRU ordering.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  parent = window1->parent();
  children_ordered_by_stacking = parent->children();
  std::reverse(children_ordered_by_stacking.begin(),
               children_ordered_by_stacking.end());
  ASSERT_TRUE(parent);
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  EXPECT_EQ(expected_mru_order, children_ordered_by_stacking);
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(parent).id());
}

// Similar to the above test but with windows created on both displays.
TEST_F(PersistentWindowControllerTest, MRUOrderMatchesStackingInterleaved) {
  UpdateDisplay("500x600,500x600");

  // Add four windows, two on each display.
  const gfx::Rect primary_bounds(200, 200);
  const gfx::Rect secondary_bounds(500, 0, 200, 200);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(primary_bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(secondary_bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(primary_bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(secondary_bounds);

  // MRU order should be opposite of the order the windows were created.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<raw_ptr<aura::Window, VectorExperimental>>
      expected_mru_order = {window4.get(), window3.get(), window2.get(),
                            window1.get()};
  ASSERT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));

  // Disconnect secondary display. The windows should move to the primary
  // display and retain MRU ordering. Note that this logic is part of
  // RootWindowController and not PersistentWindowController.
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The order which the children are stacked in is the reverse of the order
  // they are in the children() field.
  aura::Window* parent = window1->parent();
  ASSERT_TRUE(parent);
  ASSERT_EQ(parent, window2->parent());
  std::vector<raw_ptr<aura::Window, VectorExperimental>>
      children_ordered_by_stacking = parent->children();
  std::reverse(children_ordered_by_stacking.begin(),
               children_ordered_by_stacking.end());
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  EXPECT_EQ(expected_mru_order, children_ordered_by_stacking);
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(parent).id());

  // Reconnect secondary display. |window2| and |window4| should move back to
  // the secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  parent = window1->parent();
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(parent).id());
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(window1.get(), parent->children()[0]);
  EXPECT_EQ(window3.get(), parent->children()[1]);

  parent = window2->parent();
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(parent).id());
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(window2.get(), parent->children()[0]);
  EXPECT_EQ(window4.get(), parent->children()[1]);
}

// Tests that if a window is on a primary display which gets disconnected, on
// reconnect the windows bounds will be persisted.
TEST_F(PersistentWindowControllerTest, DisconnectingPrimaryDisplay) {
  // Create two displays with the one higher resolution.
  UpdateDisplay("500x600,1500x500");
  const int64_t small_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t large_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  // Set the larger display to be the primary display.
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(large_id);
  ASSERT_EQ(large_id, WindowTreeHostManager::GetPrimaryDisplayId());

  // Add a window on the larger display.
  const gfx::Rect bounds(0, 200, 1500, 200);
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);

  // Disconnect the large display. The windows should move to the new primary
  // display (small display) and shrink to fit.
  display::ManagedDisplayInfo small_info =
      display_manager()->GetDisplayInfo(small_id);
  display::ManagedDisplayInfo large_info =
      display_manager()->GetDisplayInfo(large_id);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(small_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(small_id, WindowTreeHostManager::GetPrimaryDisplayId());
  EXPECT_EQ(gfx::Size(500, 200), window->bounds().size());

  // Reconnect the large display. The window should move back and have the old
  // size.
  display_info_list.push_back(large_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(large_id, WindowTreeHostManager::GetPrimaryDisplayId());
  EXPECT_EQ(gfx::Size(1500, 200), window->bounds().size());
}

TEST_F(PersistentWindowControllerTest, RestoreBoundsOnScreenRotation) {
  UpdateDisplay("800x600");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  gfx::Rect bounds_in_landscape = gfx::Rect(420, 200, 200, 100);
  aura::Window* w1 = CreateTestWindowInShellWithBounds(bounds_in_landscape);

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  EXPECT_EQ(bounds_in_landscape, w1->GetBoundsInScreen());

  // The window should be fully visible after rotation.
  base::HistogramTester histogram_tester;
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  gfx::Rect bounds_in_portrait = w1->GetBoundsInScreen();
  EXPECT_NE(bounds_in_landscape, bounds_in_portrait);
  EXPECT_TRUE(GetPrimaryDisplay().bounds().Contains(bounds_in_portrait));
  histogram_tester.ExpectTotalCount(
      PersistentWindowController::kNumOfWindowsRestoredOnScreenRotation, 0);

  // The window's bounds should be restored after rotated back to landscape
  // primary.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  EXPECT_EQ(bounds_in_landscape, w1->GetBoundsInScreen());
  histogram_tester.ExpectTotalCount(
      PersistentWindowController::kNumOfWindowsRestoredOnScreenRotation, 1);

  // Update window's bounds in portrait primary.
  auto* window_state = WindowState::Get(w1);
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  EXPECT_EQ(bounds_in_portrait, w1->GetBoundsInScreen());
  w1->SetBounds(gfx::Rect(
      gfx::Point(bounds_in_portrait.x() - 100, bounds_in_portrait.y() - 100),
      bounds_in_portrait.size()));
  window_state->SetBoundsChangedByUser(true);
  bounds_in_portrait = w1->GetBoundsInScreen();
  EXPECT_FALSE(window_state->persistent_window_info_of_screen_rotation());

  // The window's bounds should not be restored after rotated to landscape
  // secondary, since the window's bounds has been changed by user.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  EXPECT_NE(bounds_in_landscape, w1->GetBoundsInScreen());
  bounds_in_landscape = w1->GetBoundsInScreen();

  // The window's bounds should be the same as its bounds in portrait primary
  // after rotated to portrait secondary.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  EXPECT_EQ(bounds_in_portrait, w1->GetBoundsInScreen());

  // The window's bounds should be the same as its bounds in landscape secondary
  // after rotated to landscape primary.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  EXPECT_EQ(bounds_in_landscape, w1->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, RotationOnLockScreen) {
  UpdateDisplay("800x600");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  const gfx::Rect bounds_in_landscape = gfx::Rect(420, 200, 200, 100);
  aura::Window* w1 = CreateTestWindowInShellWithBounds(bounds_in_landscape);

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);
  EXPECT_EQ(bounds_in_landscape, w1->GetBoundsInScreen());

  // Rotates to portrait primary.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);

  // Enters locked session state and rotates the screen back to landscape
  // primary.
  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);

  // Unlocks and checks that `w1` is restored.
  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  EXPECT_EQ(bounds_in_landscape, w1->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, RotationOnDisplayReconnecting) {
  UpdateDisplay("500x600,500x600");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect w1_bounds_in_landscape = gfx::Rect(200, 0, 100, 200);
  const gfx::Rect w2_bounds_in_second_display = gfx::Rect(501, 0, 200, 100);
  aura::Window* w1 = CreateTestWindowInShellWithBounds(w1_bounds_in_landscape);
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(w2_bounds_in_second_display);
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(w1_bounds_in_landscape, w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);

  // Reconnects secondary display, `w2` should be restored.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(w2_bounds_in_second_display, w2->GetBoundsInScreen());

  // Rotates the internal display back to landscape primary.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(w1_bounds_in_landscape, w1->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, NoRestoreOnRotationForSnappedWindows) {
  UpdateDisplay("800x600");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 200, 200));
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  WindowSnapWMEvent primary_snap_event(WM_EVENT_SNAP_PRIMARY);
  auto* window_state = WindowState::Get(w1);
  window_state->OnWMEvent(&primary_snap_event);
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state->GetStateType());
  const gfx::Rect bounds_in_landscape_primary = w1->GetBoundsInScreen();
  EXPECT_EQ(0, bounds_in_landscape_primary.x());

  // Snapped window should not have persistent window info on screen rotation
  // so its bounds will not be restored.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  EXPECT_FALSE(window_state->persistent_window_info_of_screen_rotation());

  // Snapped window's bounds should not be restored on screen rotation. The
  // primary snapped window in landscape primary should still be primary snapped
  // after rotated to landscape secondary, and be kept at the right side of the
  // screen.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  EXPECT_FALSE(window_state->persistent_window_info_of_screen_rotation());
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state->GetStateType());
  const gfx::Rect bounds_in_landscape_secondary = w1->GetBoundsInScreen();
  EXPECT_NE(bounds_in_landscape_primary, bounds_in_landscape_secondary);
  EXPECT_NE(0, bounds_in_landscape_secondary.x());
  EXPECT_EQ(
      bounds_in_landscape_secondary.right(),
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(w1)
          .right());
}

TEST_F(PersistentWindowControllerTest, WindowStateChangeInSamePhysicalDisplay) {
  UpdateDisplay("500x600,500x700");

  // Starts with a window in the secondary display.
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(501, 0, 200, 100));
  WindowState* window_state = WindowState::Get(window.get());
  // Maximize the window.
  window_state->Maximize();
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  ASSERT_TRUE(window_state->HasRestoreBounds());
  const gfx::Rect maximized_bounds = window->GetBoundsInScreen();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnect the primary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  // Restore the maximized window after removing the primary display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_FALSE(window_state->HasRestoreBounds());

  // Reconnect the primary display.
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  // The window should still in the secondary display, in normal state and
  // without restore bounds property set. As the window always stay in the
  // secondary display, it was never being moved to another display. Its window
  // state changes should be kept in this process.
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_FALSE(window_state->HasRestoreBounds());
  EXPECT_NE(window->GetBoundsInScreen(), maximized_bounds);
  // TODO(b/291341473): The window bounds should be {501, 0, 200, 100} based on
  // correct restore bounds updated on the display changes.
}

}  // namespace ash

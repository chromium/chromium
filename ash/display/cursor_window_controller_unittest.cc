// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/display/display_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

class CursorWindowControllerTest : public AshTestBase {
 public:
  CursorWindowControllerTest() = default;
  ~CursorWindowControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Shell hides the cursor by default; show it for these tests.
    Shell::Get()->cursor_manager()->ShowCursor();

    cursor_window_controller_ =
        Shell::Get()->window_tree_host_manager()->cursor_window_controller();
    SetCursorCompositionEnabled(true);
  }

  ui::CursorType GetCursorType() const {
    return cursor_window_controller_->cursor_.native_type();
  }

  const gfx::Point& GetCursorHotPoint() const {
    return cursor_window_controller_->hot_point_;
  }

  aura::Window* GetCursorWindow() const {
    return cursor_window_controller_->cursor_window_.get();
  }

  const gfx::ImageSkia& GetCursorImage() const {
    return cursor_window_controller_->GetCursorImageForTest();
  }

  int64_t GetCursorDisplayId() const {
    return cursor_window_controller_->display_.id();
  }

  void SetCursorCompositionEnabled(bool enabled) {
    // Cursor compositing will be enabled when high contrast mode is turned on.
    // Cursor compositing will be disabled when high contrast mode is the only
    // feature using it and is turned off.
    Shell::Get()->accessibility_controller()->SetHighContrastEnabled(enabled);
    Shell::Get()->UpdateCursorCompositingEnabled();
  }

  CursorWindowController* cursor_window_controller() {
    return cursor_window_controller_;
  }

 private:
  // Not owned.
  CursorWindowController* cursor_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(CursorWindowControllerTest);
};

// Test that the composited cursor moves to another display when the real cursor
// moves to another display.
TEST_F(CursorWindowControllerTest, MoveToDifferentDisplay) {
  UpdateDisplay("200x200,200x200*2/r");

  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();
  int64_t primary_display_id = window_tree_host_manager->GetPrimaryDisplayId();
  int64_t secondary_display_id = display_manager()->GetSecondaryDisplay().id();
  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display_id);
  aura::Window* secondary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(secondary_display_id);

  ui::test::EventGenerator primary_generator(primary_root);
  primary_generator.MoveMouseToInHost(20, 50);

  EXPECT_TRUE(primary_root->Contains(GetCursorWindow()));
  EXPECT_EQ(primary_display_id, GetCursorDisplayId());
  EXPECT_EQ(ui::CursorType::kNull, GetCursorType());
  gfx::Point hot_point = GetCursorHotPoint();
  EXPECT_EQ("4,4", hot_point.ToString());
  gfx::Rect cursor_bounds = GetCursorWindow()->GetBoundsInScreen();
  EXPECT_EQ(20, cursor_bounds.x() + hot_point.x());
  EXPECT_EQ(50, cursor_bounds.y() + hot_point.y());

  // The cursor can only be moved between displays via
  // WindowTreeHost::MoveCursorTo(). EventGenerator uses a hack to move the
  // cursor between displays.
  // Screen location: 220, 50
  // Root location: 20, 50
  secondary_root->MoveCursorTo(gfx::Point(20, 50));

  // Chrome relies on WindowTreeHost::MoveCursorTo() dispatching a mouse move
  // asynchronously. This is implemented in a platform specific way. Generate a
  // fake mouse move instead of waiting.
  gfx::Point new_cursor_position_in_host(20, 50);
  secondary_root->GetHost()->ConvertDIPToPixels(&new_cursor_position_in_host);
  ui::test::EventGenerator secondary_generator(secondary_root);
  secondary_generator.MoveMouseToInHost(new_cursor_position_in_host);

  EXPECT_TRUE(secondary_root->Contains(GetCursorWindow()));
  EXPECT_EQ(secondary_display_id, GetCursorDisplayId());
  EXPECT_EQ(ui::CursorType::kNull, GetCursorType());
  hot_point = GetCursorHotPoint();
  EXPECT_EQ("3,3", hot_point.ToString());
  cursor_bounds = GetCursorWindow()->GetBoundsInScreen();
  EXPECT_EQ(220, cursor_bounds.x() + hot_point.x());
  EXPECT_EQ(50, cursor_bounds.y() + hot_point.y());
}

// Make sure that composition cursor inherits the visibility state.
TEST_F(CursorWindowControllerTest, VisibilityTest) {
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_TRUE(GetCursorWindow()->IsVisible());
  aura::client::CursorClient* client = Shell::Get()->cursor_manager();
  client->HideCursor();
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_FALSE(GetCursorWindow()->IsVisible());

  // Normal cursor should be in the correct state.
  SetCursorCompositionEnabled(false);
  ASSERT_FALSE(GetCursorWindow());
  ASSERT_FALSE(client->IsCursorVisible());

  // Cursor was hidden.
  SetCursorCompositionEnabled(true);
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_FALSE(GetCursorWindow()->IsVisible());

  // Goback to normal cursor and show the cursor.
  SetCursorCompositionEnabled(false);
  ASSERT_FALSE(GetCursorWindow());
  ASSERT_FALSE(client->IsCursorVisible());
  client->ShowCursor();
  ASSERT_TRUE(client->IsCursorVisible());

  // Cursor was shown.
  SetCursorCompositionEnabled(true);
  ASSERT_TRUE(GetCursorWindow());
  EXPECT_TRUE(GetCursorWindow()->IsVisible());
}

// Make sure that composition cursor stays big even when
// the DSF becomes 1x as a result of zooming out.
TEST_F(CursorWindowControllerTest, DSF) {
  UpdateDisplay("1000x500*2");
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         primary_id);
  SetCursorCompositionEnabled(true);
  ASSERT_EQ(
      2.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(GetCursorImage().HasRepresentation(2.0f));

  display_manager()->UpdateZoomFactor(primary_id, 0.5f);
  ASSERT_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(GetCursorImage().HasRepresentation(2.0f));
}

// Test that cursor compositing is enabled if at least one of the features that
// use it is enabled.
TEST_F(CursorWindowControllerTest, ShouldEnableCursorCompositing) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  // Cursor compositing is disabled by default.
  SetCursorCompositionEnabled(false);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Enable large cursor, cursor compositing should be enabled.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  Shell::Get()->UpdateCursorCompositingEnabled();
  EXPECT_TRUE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Disable large cursor, cursor compositing should be disabled.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, false);
  Shell::Get()->UpdateCursorCompositingEnabled();
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());
}

}  // namespace ash

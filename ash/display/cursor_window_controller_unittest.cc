// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/display_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_lookup.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/layout.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"
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

  ui::mojom::CursorType GetCursorType() const {
    return cursor_window_controller_->cursor_.type();
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

  void SetCursorSize(ui::CursorSize size) {
    cursor_window_controller_->SetCursorSize(size);
  }

  void SetLargeCursorSizeInDip(int large_cursor_size_in_dip) {
    cursor_window_controller_->SetLargeCursorSizeInDip(
        large_cursor_size_in_dip);
  }

  void SetCursorCompositionEnabled(bool enabled) {
    // Cursor compositing will be enabled when high contrast mode is turned on.
    // Cursor compositing will be disabled when high contrast mode is the only
    // feature using it and is turned off.
    Shell::Get()->accessibility_controller()->high_contrast().SetEnabled(
        enabled);
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
  int64_t secondary_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display_id);
  aura::Window* secondary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(secondary_display_id);

  ui::test::EventGenerator primary_generator(primary_root);
  primary_generator.MoveMouseToInHost(20, 50);

  EXPECT_TRUE(primary_root->Contains(GetCursorWindow()));
  EXPECT_EQ(primary_display_id, GetCursorDisplayId());
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCursorType());
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
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCursorType());
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

// Test that the composited cursor matches the native cursor in size.
TEST_F(CursorWindowControllerTest, CursorSize) {
  for (int dsf : {1, 2}) {
    std::string display_specs = base::StringPrintf("1000x500*%d", dsf);
    UpdateDisplay(display_specs);
    int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
    display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                           primary_id);
    for (float zoom_factor : {0.5f, 1.0f, 1.5f, 2.0f}) {
      display_manager()->UpdateZoomFactor(primary_id, zoom_factor);
      auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
      const float ui_scale = display.device_scale_factor();

      ui::Cursor cursor(ui::mojom::CursorType::kPointer);
      // This would be done by NativeCursorManagerAsh::SetCursor().
      cursor.set_image_scale_factor(ui::GetScaleForResourceScaleFactor(
          ui::GetSupportedScaleFactor(ui_scale)));

      // Normal size.
      SetCursorSize(ui::CursorSize::kNormal);

      // Both checks are needed. To ensure that the cursor isn't scaled, besides
      // checking that its size isn't changed, the image needs to be available
      // for the current display scale.
      EXPECT_EQ(GetCursorImage().GetRepresentation(ui_scale).pixel_size(),
                gfx::SkISizeToSize(ui::GetCursorBitmap(cursor).dimensions()));
      EXPECT_TRUE(GetCursorImage().HasRepresentation(ui_scale));

      // Large size.
      SetCursorSize(ui::CursorSize::kLarge);
      for (const auto kLargeSizeInDips :
           {kDefaultLargeCursorSize / 2, kDefaultLargeCursorSize}) {
        SetLargeCursorSizeInDip(kLargeSizeInDips);
        EXPECT_EQ(
            GetCursorImage().GetRepresentation(ui_scale).pixel_size().width(),
            kLargeSizeInDips * dsf);
        EXPECT_TRUE(GetCursorImage().HasRepresentation(ui_scale));
      }

      // TODO(https://crbug.com/1149906): test custom cursors when
      // WebCursor::GetNativeCursor() is moved to CursorLoader, as ash cannot
      // depend on //content.
    }
  }
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
  EXPECT_TRUE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Disable large cursor, cursor compositing should be disabled.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, false);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());
}

TEST_F(CursorWindowControllerTest, CursorColoringSpotCheck) {
  SetCursorCompositionEnabled(false);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Try a few colors to ensure colorizing is working appropriately.
  const struct {
    SkColor cursor_color;  // Set the cursor to this color.
    SkColor not_found;     // Spot-check: This color shouldn't be in the cursor.
    SkColor found;         // Spot-check: This color should be in the cursor.
    gfx::NativeCursor cursor;
  } kTestCases[] = {
      // Cursors should still have white.
      {SK_ColorMAGENTA, SK_ColorBLUE, SK_ColorWHITE,
       ui::mojom::CursorType::kHand},
      {SK_ColorBLUE, SK_ColorMAGENTA, SK_ColorWHITE,
       ui::mojom::CursorType::kCell},
      {SK_ColorGREEN, SK_ColorBLUE, SK_ColorWHITE,
       ui::mojom::CursorType::kNoDrop},
      // Also cursors should still have transparent.
      {SK_ColorRED, SK_ColorGREEN, SK_ColorTRANSPARENT,
       ui::mojom::CursorType::kPointer},
      // The no drop cursor has red in it, check it's still there:
      // Most of the cursor should be colored, but the red part shouldn't be
      // re-colored.
      {SK_ColorBLUE, SK_ColorGREEN, SkColorSetRGB(173, 8, 8),
       ui::mojom::CursorType::kNoDrop},
      // Similarly, the copy cursor has green in it.
      {SK_ColorBLUE, SK_ColorRED, SkColorSetRGB(19, 137, 16),
       ui::mojom::CursorType::kCopy},
  };

  for (const auto& test : kTestCases) {
    // Setting a color enables cursor compositing.
    cursor_window_controller()->SetCursorColor(test.cursor_color);
    Shell::Get()->UpdateCursorCompositingEnabled();
    EXPECT_TRUE(cursor_window_controller()->is_cursor_compositing_enabled());
    cursor_window_controller()->SetCursor(test.cursor);
    const SkBitmap* bitmap = GetCursorImage().bitmap();
    // We should find |cursor_color| pixels in the cursor, but no black or
    // |not_found| color pixels. All black pixels are recolored.
    // We should also find |found| color.
    bool has_color = false;
    bool has_not_found_color = false;
    bool has_found_color = false;
    bool has_black = false;
    for (int x = 0; x < bitmap->width(); ++x) {
      for (int y = 0; y < bitmap->height(); ++y) {
        SkColor color = bitmap->getColor(x, y);
        if (color == test.cursor_color)
          has_color = true;
        else if (color == test.not_found)
          has_not_found_color = true;
        else if (color == test.found)
          has_found_color = true;
        else if (color == SK_ColorBLACK)
          has_black = true;
      }
    }
    EXPECT_TRUE(has_color) << color_utils::SkColorToRgbaString(
        test.cursor_color);
    EXPECT_TRUE(has_found_color)
        << color_utils::SkColorToRgbaString(test.found);
    EXPECT_FALSE(has_not_found_color)
        << color_utils::SkColorToRgbaString(test.not_found);
    EXPECT_FALSE(has_black);
  }

  // Set back to the default color and ensure cursor compositing is disabled.
  cursor_window_controller()->SetCursorColor(kDefaultCursorColor);
  Shell::Get()->UpdateCursorCompositingEnabled();
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());
}

}  // namespace ash

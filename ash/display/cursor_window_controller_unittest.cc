// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"

#include <cmath>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/display_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

float DistanceBetweenPoints(const gfx::Point& p1, const gfx::Point& p2) {
  float x_diff = p1.x() - p2.x();
  float y_diff = p1.y() - p2.y();
  return std::sqrt(x_diff * x_diff + y_diff * y_diff);
}

float DistanceBetweenSizes(const gfx::Size& s1, const gfx::Size& s2) {
  float width_diff = s1.width() - s2.width();
  float height_diff = s1.height() - s2.height();
  return std::sqrt(width_diff * width_diff + height_diff * height_diff);
}

}  // namespace

using ::ui::mojom::CursorType;

class CursorWindowControllerTest : public AshTestBase {
 public:
  CursorWindowControllerTest() = default;

  CursorWindowControllerTest(const CursorWindowControllerTest&) = delete;
  CursorWindowControllerTest& operator=(const CursorWindowControllerTest&) =
      delete;

  ~CursorWindowControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Shell hides the cursor by default; show it for these tests.
    Shell::Get()->cursor_manager()->ShowCursor();

    SetCursorCompositionEnabled(true);
  }

  CursorType GetCursorType() const {
    return cursor_window_controller()->cursor_.type();
  }

  const gfx::Rect GetCursorBounds() const {
    return cursor_window_controller()->GetCursorBoundsInScreenForTest();
  }

  const gfx::Point& GetCursorHotPoint() const {
    return cursor_window_controller()->hot_point_;
  }

  const aura::Window* GetCursorHostWindow() const {
    return cursor_window_controller()->GetCursorHostWindowForTest();
  }

  const gfx::ImageSkia& GetCursorImage() const {
    return cursor_window_controller()->GetCursorImageForTest();
  }

  int64_t GetCursorDisplayId() const {
    return cursor_window_controller()->display_.id();
  }

  void SetCursorCompositionEnabled(bool enabled) {
    // Cursor compositing will be enabled when high contrast mode is turned on.
    // Cursor compositing will be disabled when high contrast mode is the only
    // feature using it and is turned off.
    Shell::Get()->accessibility_controller()->high_contrast().SetEnabled(
        enabled);
  }

  CursorWindowController* cursor_window_controller() const {
    return Shell::Get()->window_tree_host_manager()->cursor_window_controller();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the composited cursor moves to another display when the real cursor
// moves to another display.
TEST_F(CursorWindowControllerTest, MoveToDifferentDisplay) {
  UpdateDisplay("300x200,300x200*2/r");

  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();
  int64_t primary_display_id = window_tree_host_manager->GetPrimaryDisplayId();
  int64_t secondary_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display_id);

  ui::test::EventGenerator primary_generator(primary_root);
  primary_generator.MoveMouseToInHost(20, 50);

  EXPECT_TRUE(primary_root->Contains(GetCursorHostWindow()));
  EXPECT_EQ(primary_display_id, GetCursorDisplayId());
  EXPECT_EQ(CursorType::kNull, GetCursorType());
  gfx::Point hot_point = GetCursorHotPoint();
  EXPECT_EQ(gfx::Point(6, 4), hot_point);
  gfx::Rect cursor_bounds = GetCursorBounds();
  EXPECT_EQ(20, cursor_bounds.x() + hot_point.x());
  EXPECT_EQ(50, cursor_bounds.y() + hot_point.y());

  // Move to secondary.
  AshWindowTreeHost* secondary_ash_wth =
      window_tree_host_manager->GetAshWindowTreeHostForDisplayId(
          secondary_display_id);
  auto* secondary_wth = secondary_ash_wth->AsWindowTreeHost();
  auto* secondary_root = secondary_wth->window();

  MoveCursorTo(secondary_ash_wth, gfx::Point(299, 50), true);

  // Chrome relies on WindowTreeHost::MoveCursorToInternal() dispatching a mouse
  // move asynchronously. This is implemented in a platform specific way.
  // Generate a fake mouse move instead of waiting.
  auto& pos = aura::test::QueryLatestMousePositionRequestInHost(secondary_wth);
  ui::test::EventGenerator secondary_generator(secondary_root);
  secondary_generator.MoveMouseToInHost(pos);

  EXPECT_TRUE(secondary_root->Contains(GetCursorHostWindow()));
  EXPECT_EQ(secondary_display_id, GetCursorDisplayId());
  EXPECT_EQ(CursorType::kNull, GetCursorType());
  hot_point = GetCursorHotPoint();
  EXPECT_EQ(gfx::Point(6, 4), hot_point);
  cursor_bounds = GetCursorBounds();
  EXPECT_EQ(300, cursor_bounds.x() + hot_point.x());
  EXPECT_EQ(50, cursor_bounds.y() + hot_point.y());
}

// Make sure that composition cursor inherits the visibility state.
TEST_F(CursorWindowControllerTest, VisibilityTest) {
  ASSERT_TRUE(GetCursorHostWindow());
  EXPECT_TRUE(GetCursorHostWindow()->IsVisible());
  aura::client::CursorClient* client = Shell::Get()->cursor_manager();
  client->HideCursor();
  ASSERT_TRUE(GetCursorHostWindow());
  EXPECT_FALSE(GetCursorHostWindow()->IsVisible());

  // Normal cursor should be in the correct state.
  SetCursorCompositionEnabled(false);
  ASSERT_FALSE(GetCursorHostWindow());
  ASSERT_FALSE(client->IsCursorVisible());

  // Cursor was hidden.
  SetCursorCompositionEnabled(true);
  ASSERT_TRUE(GetCursorHostWindow());
  EXPECT_FALSE(GetCursorHostWindow()->IsVisible());

  // Goback to normal cursor and show the cursor.
  SetCursorCompositionEnabled(false);
  ASSERT_FALSE(GetCursorHostWindow());
  ASSERT_FALSE(client->IsCursorVisible());
  client->ShowCursor();
  ASSERT_TRUE(client->IsCursorVisible());

  // Cursor was shown.
  SetCursorCompositionEnabled(true);
  ASSERT_TRUE(GetCursorHostWindow());
  EXPECT_TRUE(GetCursorHostWindow()->IsVisible());
}

// Test different properties of the composited cursor with different device
// scale factors and zoom levels.
TEST_F(CursorWindowControllerTest, DSF) {
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();

  auto cursor_test = [&](ui::Cursor cursor, float large_cursor_size_in_dip) {
    const float dsf =
        display::Screen::Get()->GetPrimaryDisplay().device_scale_factor();
    SCOPED_TRACE(testing::Message()
                 << cursor.type() << " at scale " << dsf << " and size "
                 << large_cursor_size_in_dip);

    cursor_window_controller()->SetCursor(cursor);
    const std::optional<ui::CursorData> cursor_data =
        cursor_shape_client.GetCursorData(cursor);
    DCHECK(cursor_data);

    // Software cursors look blurry if they are resized by the window they are
    // rendered in, instead of by `ImageSkia`. Make sure
    // `CursorWindowController` creates the cursor in a way that a
    // representation for the display's device scale factor can be directly
    // obtained.
    const gfx::ImageSkiaRep& rep = GetCursorImage().GetRepresentation(dsf);
    EXPECT_EQ(rep.scale(), dsf);

    const gfx::Size original_cursor_size_in_dip =
        // ImageSkiaRep::GetWidth() uses static_cast<int>.
        gfx::ToFlooredSize(gfx::ConvertSizeToDips(
            gfx::SkISizeToSize(cursor_data->bitmaps[0].dimensions()),
            cursor_data->scale_factor));
    const gfx::Size cursor_size_in_dip =
        large_cursor_size_in_dip != 0
            ? gfx::Size(large_cursor_size_in_dip, large_cursor_size_in_dip)
            : original_cursor_size_in_dip;
    // Scaling operations and conversions between dp and px can cause rounding
    // errors. We accept rounding errors <= sqrt(1+1).
    EXPECT_LE(DistanceBetweenSizes(GetCursorImage().size(), cursor_size_in_dip),
              sqrt(2));

    // TODO(hferreiro): the cursor hotspot for non-custom cursors cannot be
    // checked, since the software cursor uses
    // `ui::GetSupportedResourceScaleFactorForRescale`, and
    // `CursorLoader::GetCursorData` uses `ui::GetSupportedResourceScaleFactor`,
    // and 2x cursor hotspots are not just twice the 1x hotspots.
    if (cursor.type() == CursorType::kCustom) {
      const gfx::Point hotspot_in_dip =
          gfx::ToFlooredPoint(gfx::ConvertPointToDips(
              cursor_data->hotspot, cursor_data->scale_factor));
      const float rescale = static_cast<float>(cursor_size_in_dip.height()) /
                            original_cursor_size_in_dip.height();
      // Scaling operations and conversions between dp and px can cause rounding
      // errors. We accept rounding errors <= sqrt(1+1).
      ASSERT_LE(DistanceBetweenPoints(
                    GetCursorHotPoint(),
                    gfx::ScaleToCeiledPoint(hotspot_in_dip, rescale)),
                sqrt(2));
    }

    // The cursor window should have the same size as the cursor.
    EXPECT_EQ(GetCursorBounds().size(), GetCursorImage().size());
  };

  auto* const cursor_manager = Shell::Get()->cursor_manager();
  DCHECK(cursor_manager);

  for (const float device_scale_factor : {1.0f, 1.5f, 2.0f, 2.5f}) {
    for (const float zoom : {0.8f, 1.0f, 1.25f}) {
      UpdateDisplay(
          base::StringPrintf("1000x500*%f@%f", device_scale_factor, zoom));
      const float dsf =
          display::Screen::Get()->GetPrimaryDisplay().device_scale_factor();

      for (const int large_cursor_size_in_dip : {0, 32, 64, 128}) {
        cursor_manager->SetCursorSize(large_cursor_size_in_dip == 0
                                          ? ui::CursorSize::kNormal
                                          : ui::CursorSize::kLarge);
        Shell::Get()->SetLargeCursorSizeInDip(large_cursor_size_in_dip);

        // Default cursor.
        cursor_test(CursorType::kPointer, large_cursor_size_in_dip);

        // Custom cursor. Custom cursors are always scaled at the device scale
        // factor. See `WebCursor::GetNativeCursor`.
        cursor_test(ui::Cursor::NewCustom(gfx::test::CreateBitmap(/*size=*/20),
                                          gfx::Point(10, 10), dsf),
                    large_cursor_size_in_dip);
      }
    }
  }
}

// Test that cursor compositing is enabled if at least one of the features that
// use it is enabled.
TEST_F(CursorWindowControllerTest, ShouldEnableCursorCompositing) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  display::Display display = display::Screen::Get()->GetPrimaryDisplay();
  const float dsf = 2.0f;
  display.set_device_scale_factor(dsf);
  display.set_maximum_cursor_size(gfx::Size(128, 128));
  const gfx::SizeF display_maximum_cursor_size_in_dip =
      gfx::ConvertSizeToDips(display.maximum_cursor_size(), dsf);

  // Cursor compositing is disabled by default.
  SetCursorCompositionEnabled(false);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Enable high contrast feature, cursor compositing should be enabled.
  prefs->SetBoolean(prefs::kAccessibilityHighContrastEnabled, true);
  EXPECT_TRUE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Disable high contrast feature, cursor compositing should be disabled.
  prefs->SetBoolean(prefs::kAccessibilityHighContrastEnabled, false);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Enable docked magnifier feature, cursor compositing should be enabled.
  prefs->SetBoolean(prefs::kDockedMagnifierEnabled, true);
  EXPECT_TRUE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Disable docked magnifier feature, cursor compositing should be disabled.
  prefs->SetBoolean(prefs::kDockedMagnifierEnabled, false);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Enable large cursor, cursor compositing should be enabled only if
  // the target large cursor size is larger than current display's maximum
  // cursor size.
  cursor_window_controller()->SetDisplay(display);
  cursor_window_controller()->SetLargeCursorSizeInDip(
      display_maximum_cursor_size_in_dip.height() + 1);
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  EXPECT_TRUE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Disable large cursor, cursor compositing should be disabled.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, false);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Re-enable large cursor, since large cursor size is smaller than display's
  // maximum cursor size, cursor should use hardware compositing instead of
  // software compositing.
  cursor_window_controller()->SetDisplay(display);
  cursor_window_controller()->SetLargeCursorSizeInDip(
      display_maximum_cursor_size_in_dip.height() - 1);
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  EXPECT_FALSE(cursor_window_controller()->is_cursor_compositing_enabled());
}

// Test that cursor color works correctly when large cursor is enabled.
TEST_F(CursorWindowControllerTest, LargeCursorColoringSpotCheck) {
  // Enable large cursor, cursor compositing should be enabled and thus
  // colored cursor should also be composited cursor.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  ASSERT_TRUE(cursor_window_controller()->is_cursor_compositing_enabled());

  // Try a few colors to ensure colorizing is working appropriately.
  const struct {
    SkColor cursor_color;  // Set the cursor to this color.
    SkColor not_found;     // Spot-check: This color shouldn't be in the cursor.
    SkColor found;         // Spot-check: This color should be in the cursor.
    CursorType cursor_type;
  } kColorTestCases[] = {
      // Cursors should not have black because the black cursor body should be
      // replaced by other colors.
      {SK_ColorRED, SK_ColorBLACK, SK_ColorWHITE, CursorType::kPointer},
      {SK_ColorRED, SK_ColorBLACK, SK_ColorWHITE, CursorType::kCell},

      // Cursors should not have white because the white cursor body should be
      // replaced by other colors.
      {SK_ColorMAGENTA, SK_ColorWHITE, SK_ColorBLACK, CursorType::kHand},

      // Also cursors should still have transparent.
      {SK_ColorRED, SK_ColorGREEN, SK_ColorTRANSPARENT, CursorType::kPointer},

      // The no drop cursor has red in it, check it's still there:
      // Cursor body should be colored, but the red part shouldn't be
      // re-colored.
      {SK_ColorBLUE, SK_ColorGREEN, SkColorSetRGB(181, 70, 72),
       CursorType::kNoDrop},

      // Similarly, the copy cursor has a green part in it which should not be
      // re-colored.
      {SK_ColorBLUE, SK_ColorRED, SkColorSetRGB(57, 149, 88),
       CursorType::kCopy},
  };

  for (const auto& test : kColorTestCases) {
    cursor_window_controller()->SetCursorColor(test.cursor_color);
    cursor_window_controller()->SetCursor(test.cursor_type);
    const SkBitmap* bitmap = GetCursorImage().bitmap();
    // We should find `cursor_color` and `found` color pixels in the cursor, but
    // no |not_found| color pixels.
    bool has_color = false;
    bool has_not_found_color = false;
    bool has_found_color = false;
    for (int x = 0; x < bitmap->width(); ++x) {
      for (int y = 0; y < bitmap->height(); ++y) {
        SkColor color = bitmap->getColor(x, y);
        if (color == test.cursor_color)
          has_color = true;
        else if (color == test.not_found)
          has_not_found_color = true;
        else if (color == test.found)
          has_found_color = true;
      }
    }
    EXPECT_TRUE(has_color) << color_utils::SkColorToRgbaString(
        test.cursor_color);
    EXPECT_TRUE(has_found_color)
        << color_utils::SkColorToRgbaString(test.found);
    EXPECT_FALSE(has_not_found_color)
        << color_utils::SkColorToRgbaString(test.not_found);
  }

  // Set back to the default color and ensure cursor compositing is disabled.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, false);
  SetCursorCompositionEnabled(false);
}

TEST_F(CursorWindowControllerTest, RefreshRateChangeUpdatesMaxUpdateRates) {
  const int64_t display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::Display display = display::Screen::Get()->GetPrimaryDisplay();
  auto display_bounds = display.bounds();

  // Default refresh rate is 60.
  EXPECT_NEAR(cursor_window_controller()->max_update_rate_ms(), 11.11, 0.01f);

  // Change refresh rate to 30.
  float refresh_rate = 30.f;
  display::ManagedDisplayInfo info =
      display::CreateDisplayInfo(display_id, display_bounds);
  info.set_refresh_rate(refresh_rate);
  display::ManagedDisplayMode mode(display_bounds.size(), refresh_rate,
                                   /*is_interlaced=*/false,
                                   /*native=*/true);
  info.SetManagedDisplayModes({mode});

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_NEAR(cursor_window_controller()->max_update_rate_ms(), 22.22, 0.01f);
}

}  // namespace ash

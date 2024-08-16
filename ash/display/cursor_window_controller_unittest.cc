// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cursor_window_controller.h"
#include "base/memory/raw_ptr.h"

#include <cmath>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/display_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/cursor_shape_client.h"
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
  aura::Window* secondary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(secondary_display_id);

  ui::test::EventGenerator primary_generator(primary_root);
  primary_generator.MoveMouseToInHost(20, 50);

  EXPECT_TRUE(primary_root->Contains(GetCursorHostWindow()));
  EXPECT_EQ(primary_display_id, GetCursorDisplayId());
  EXPECT_EQ(CursorType::kNull, GetCursorType());
  gfx::Point hot_point = GetCursorHotPoint();
  EXPECT_EQ(gfx::Point(4, 4), hot_point);
  gfx::Rect cursor_bounds = GetCursorBounds();
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

  EXPECT_TRUE(secondary_root->Contains(GetCursorHostWindow()));
  EXPECT_EQ(secondary_display_id, GetCursorDisplayId());
  EXPECT_EQ(CursorType::kNull, GetCursorType());
  hot_point = GetCursorHotPoint();
  EXPECT_EQ(gfx::Point(3, 3), hot_point);
  cursor_bounds = GetCursorBounds();
  EXPECT_EQ(320, cursor_bounds.x() + hot_point.x());
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

namespace {

// Emulates the behavior of BitmapImageSource used in ResourceBundle.
class TestCursorImageSource : public gfx::ImageSkiaSource {
 public:
  TestCursorImageSource() = default;
  TestCursorImageSource(const TestCursorImageSource&) = delete;
  TestCursorImageSource operator=(const TestCursorImageSource&) = delete;
  ~TestCursorImageSource() override = default;

  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    float resource_scale = ui::GetSupportedResourceScaleFactor(scale);
    if (resource_scale == 1.f) {
      return rep_1x_;
    } else if (resource_scale == 2.f) {
      return rep_2x_;
    }
    NOTREACHED();
  }

 private:
  gfx::ImageSkiaRep rep_1x_ =
      gfx::ImageSkiaRep(gfx::test::CreateBitmap(/*size=*/25, SK_ColorBLACK),
                        1.f);
  gfx::ImageSkiaRep rep_2x_ =
      gfx::ImageSkiaRep(gfx::test::CreateBitmap(/*size=*/50, SK_ColorWHITE),
                        2.f);
};

}  // namespace

// Make sure that composition cursor uses correct assets with various scales.
TEST_F(CursorWindowControllerTest, ScaleUsesCorrectAssets) {
  testing::NiceMock<ui::MockResourceBundleDelegate> mock_delegate;
  gfx::ImageSkia image_skia(std::make_unique<TestCursorImageSource>(),
                            gfx::Size(25, 25));

  auto get_pixel_value = [&](float scale) {
    // TODO(b/318592117): don't need to update display when
    // wm::GetCursorData uses ImageSkia instead of SkBitmap.
    // Trigger regeneration of the cursor image.
    UpdateDisplay(base::StringPrintf("300x200*%f", scale));

    uint32_t* data = static_cast<uint32_t*>(
        GetCursorImage().GetRepresentation(scale).GetBitmap().getPixels());
    return data[0];
  };

  EXPECT_CALL(mock_delegate, GetImageNamed(testing::_))
      .WillOnce(testing::Return(gfx::Image(image_skia)));

  ui::ResourceBundle test_bundle(&mock_delegate);
  auto* original =
      ui::ResourceBundle::SwapSharedInstanceForTesting(&test_bundle);
  // Force re-create composited cursor.
  SetCursorCompositionEnabled(false);
  SetCursorCompositionEnabled(true);

  // The cursor should use 2x resources when dsf > 1.2.
  EXPECT_EQ(SK_ColorWHITE, get_pixel_value(2.4f));
  EXPECT_EQ(SK_ColorWHITE, get_pixel_value(2.f));
  EXPECT_EQ(SK_ColorWHITE, get_pixel_value(1.25f));
  EXPECT_EQ(SK_ColorBLACK, get_pixel_value(1.20f));
  EXPECT_EQ(SK_ColorBLACK, get_pixel_value(1.15f));
  EXPECT_EQ(SK_ColorBLACK, get_pixel_value(1.f));
  EXPECT_EQ(SK_ColorBLACK, get_pixel_value(0.8f));

  ui::ResourceBundle::SwapSharedInstanceForTesting(original);
}

// Test different properties of the composited cursor with different device
// scale factors and zoom levels.
TEST_F(CursorWindowControllerTest, DSF) {
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();

  auto cursor_test = [&](ui::Cursor cursor, float size, float cursor_scale) {
    const float dsf =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
    SCOPED_TRACE(testing::Message() << cursor.type() << " at scale " << dsf
                                    << " and size " << size);

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

    const gfx::Size kOriginalCursorSize =
        // ImageSkiaRep::GetWidth() uses static_cast<int>.
        gfx::ToFlooredSize(gfx::ConvertSizeToDips(
            gfx::SkISizeToSize(cursor_data->bitmaps[0].dimensions()),
            cursor_scale));
    const gfx::Size kCursorSize =
        size != 0 ? gfx::Size(size, size) : kOriginalCursorSize;
    // Scaling operations and conversions between dp and px can cause rounding
    // errors. We accept rounding errors <= sqrt(1+1).
    EXPECT_LE(DistanceBetweenSizes(GetCursorImage().size(), kCursorSize),
              sqrt(2));

    // TODO(hferreiro): the cursor hotspot for non-custom cursors cannot be
    // checked, since the software cursor uses
    // `ui::GetSupportedResourceScaleFactorForRescale`, and
    // `CursorLoader::GetCursorData` uses `ui::GetSupportedResourceScaleFactor`,
    // and 2x cursor hotspots are not just twice the 1x hotspots.
    if (cursor.type() == CursorType::kCustom) {
      const gfx::Point kHotspot = gfx::ToFlooredPoint(
          gfx::ConvertPointToDips(cursor_data->hotspot, cursor_scale));
      const float rescale =
          static_cast<float>(kCursorSize.width()) / kOriginalCursorSize.width();
      // Scaling operations and conversions between dp and px can cause rounding
      // errors. We accept rounding errors <= sqrt(1+1).
      EXPECT_LE(
          DistanceBetweenPoints(GetCursorHotPoint(),
                                gfx::ScaleToCeiledPoint(kHotspot, rescale)),
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
      const float dsf = display::Screen::GetScreen()
                            ->GetPrimaryDisplay()
                            .device_scale_factor();

      for (const int size : {0, 32, 64, 128}) {
        cursor_manager->SetCursorSize(size == 0 ? ui::CursorSize::kNormal
                                                : ui::CursorSize::kLarge);
        Shell::Get()->SetLargeCursorSizeInDip(size);

        // Default cursor.
        cursor_test(CursorType::kPointer, size,
                    // Use the nearest resource scale factor.
                    ui::GetScaleForResourceScaleFactor(
                        ui::GetSupportedResourceScaleFactor(dsf)));

        // Custom cursor. Custom cursors are always scaled at the device scale
        // factor. See `WebCursor::GetNativeCursor`.
        cursor_test(ui::Cursor::NewCustom(gfx::test::CreateBitmap(/*size=*/20),
                                          gfx::Point(10, 10), dsf),
                    size, dsf);
      }
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
      {SK_ColorMAGENTA, SK_ColorBLUE, SK_ColorWHITE, CursorType::kHand},
      {SK_ColorBLUE, SK_ColorMAGENTA, SK_ColorWHITE, CursorType::kCell},
      {SK_ColorGREEN, SK_ColorBLUE, SK_ColorWHITE, CursorType::kNoDrop},
      // Also cursors should still have transparent.
      {SK_ColorRED, SK_ColorGREEN, SK_ColorTRANSPARENT, CursorType::kPointer},
      // The no drop cursor has red in it, check it's still there:
      // Most of the cursor should be colored, but the red part shouldn't be
      // re-colored.
      {SK_ColorBLUE, SK_ColorGREEN, SkColorSetRGB(173, 8, 8),
       CursorType::kNoDrop},
      // Similarly, the copy cursor has green in it.
      {SK_ColorBLUE, SK_ColorRED, SkColorSetRGB(19, 137, 16),
       CursorType::kCopy},
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

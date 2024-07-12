// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/wm/snap_group/snap_group_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

// Visual regression tests for Snap Groups feature, comparing visuals against
// established benchmarks.
class SnapGroupPixelTest : public AshTestBase {
 public:
  SnapGroupPixelTest() = default;
  SnapGroupPixelTest(const SnapGroupPixelTest&) = delete;
  SnapGroupPixelTest& operator=(const SnapGroupPixelTest&) = delete;
  ~SnapGroupPixelTest() override = default;

 private:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList scoped_feature_list_{features::kSnapGroup};
};

// Visual regression test for divider component (default and hover states).
TEST_F(SnapGroupPixelTest, SnapGroupDividerBasic) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  DecorateWindow(w1.get(), u"w1", SK_ColorGREEN);
  auto* w1_widget = views::Widget::GetWidgetForNativeView(w1.get());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DecorateWindow(w2.get(), u"w2", SK_ColorBLUE);
  auto* w2_widget = views::Widget::GetWidgetForNativeView(w2.get());

  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  auto* divider_widget = snap_group_divider()->divider_widget();
  ASSERT_TRUE(divider_widget);

  // Verify the snap group divider UI components on default state.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "snap_group_divider_default_state",
      /*revision_number=*/0, divider_widget, w1_widget, w2_widget));

  // Move the mouse to the position that is a off the center(divider handler
  // view) and verify the snap group divider UI components on hover state.
  event_generator->MoveMouseTo(
      snap_group_divider_bounds_in_screen().CenterPoint() +
      gfx::Vector2d(0, 10));
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "snap_group_divider_hover_state",
      /*revision_number=*/0, divider_widget, w1_widget, w2_widget));
}

}  // namespace ash

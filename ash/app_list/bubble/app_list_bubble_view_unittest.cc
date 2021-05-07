// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/bubble/app_list_bubble.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

using views::Widget;

namespace ash {
namespace {

// Distance under which two points are considered "near" each other.
constexpr int kNearDistanceDips = 20;

// The exact position of a bubble relative to its anchor is an implementation
// detail, so tests assert that points are "near" each other. This also makes
// the tests less fragile if padding changes.
testing::AssertionResult IsNear(const gfx::Point& a, const gfx::Point& b) {
  gfx::Vector2d delta = a - b;
  float distance = delta.Length();
  if (distance < float{kNearDistanceDips})
    return testing::AssertionSuccess();

  return testing::AssertionFailure()
         << a.ToString() << " is more than " << kNearDistanceDips
         << " dips away from " << b.ToString();
}

class AppListBubbleViewTest : public AshTestBase {
 public:
  AppListBubbleViewTest() {
    scoped_features_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~AppListBubbleViewTest() override = default;

  AppListBubble* GetAppListBubble() {
    return Shell::Get()->app_list_controller()->app_list_bubble_for_test();
  }

  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AppListBubbleViewTest, BubbleOpensInBottomLeftForBottomShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  Widget* widget = bubble->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_left(),
                     GetPrimaryDisplay().work_area().bottom_left()));
}

TEST_F(AppListBubbleViewTest, BubbleOpensInTopLeftForLeftShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  Widget* widget = bubble->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().origin(),
                     GetPrimaryDisplay().work_area().origin()));
}

TEST_F(AppListBubbleViewTest, BubbleOpensInTopRightForRightShelf) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  Widget* widget = bubble->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().top_right(),
                     GetPrimaryDisplay().work_area().top_right()));
}

TEST_F(AppListBubbleViewTest, BubbleOpensInBottomRightForBottomShelfRTL) {
  base::test::ScopedRestoreICUDefaultLocale locale("he");
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  Widget* widget = bubble->bubble_widget_for_test();
  EXPECT_TRUE(IsNear(widget->GetWindowBoundsInScreen().bottom_right(),
                     GetPrimaryDisplay().work_area().bottom_right()));
}

}  // namespace
}  // namespace ash

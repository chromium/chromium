// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_highlight_view.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_highlight_view_test_api.h"
#include "base/test/icu_test_util.h"
#include "ui/gfx/transform.h"

namespace ash {

namespace {

gfx::Transform GetTransform(views::View* view) {
  DCHECK(view && view->layer());
  return view->layer()->transform();
}

}  // namespace

class SplitViewHighlightViewTest : public AshTestBase {
 public:
  SplitViewHighlightViewTest() = default;
  ~SplitViewHighlightViewTest() override = default;

  SplitViewHighlightView* left_highlight() { return left_highlight_.get(); }
  SplitViewHighlightView* right_highlight() { return right_highlight_.get(); }

  // test::AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    left_highlight_ = std::make_unique<SplitViewHighlightView>(false);
    right_highlight_ = std::make_unique<SplitViewHighlightView>(true);
  }

 private:
  std::unique_ptr<SplitViewHighlightView> left_highlight_;
  std::unique_ptr<SplitViewHighlightView> right_highlight_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewHighlightViewTest);
};

// Tests setting and animating bounds for the split view highlight view in
// landscape mode.
TEST_F(SplitViewHighlightViewTest, LandscapeBounds) {
  const gfx::Rect bounds(0, 0, 100, 100);
  left_highlight()->SetBounds(bounds, /*landscape=*/true,
                              /*animation_type=*/base::nullopt);

  // Tests that setting bounds without animations in landscape mode will set the
  // bounds of the components correctly, without any transforms.
  SplitViewHighlightViewTestApi test_api(left_highlight());
  EXPECT_EQ(gfx::Rect(0, 0, 14, 100), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(4, 0, 92, 100), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(86, 0, 14, 100), test_api.GetRightBottomView()->bounds());
  EXPECT_TRUE(GetTransform(test_api.GetLeftTopView()).IsIdentity());
  EXPECT_TRUE(GetTransform(test_api.GetMiddleView()).IsIdentity());
  EXPECT_TRUE(GetTransform(test_api.GetRightBottomView()).IsIdentity());

  // Tests that after animating to new bounds, the components have the same
  // bounds, but have transforms.
  const gfx::Rect new_bounds(0, 0, 200, 100);
  left_highlight()->SetBounds(
      new_bounds, /*landscape=*/true, /*animation_type=*/
      base::make_optional(SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(gfx::Rect(0, 0, 14, 100), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(4, 0, 92, 100), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(86, 0, 14, 100), test_api.GetRightBottomView()->bounds());
  EXPECT_TRUE(GetTransform(test_api.GetLeftTopView()).IsIdentity());
  gfx::Transform expected_middle_transform;
  expected_middle_transform.Scale(2.16, 1);
  EXPECT_TRUE(GetTransform(test_api.GetMiddleView())
                  .ApproximatelyEqual(expected_middle_transform));
  gfx::Transform expected_end_transform;
  expected_end_transform.Translate(100, 0);
  EXPECT_TRUE(GetTransform(test_api.GetRightBottomView())
                  .ApproximatelyEqual(expected_end_transform));
}

// Tests setting and animating bounds for the split view highlight view in
// landscape mode for rtl languages.
TEST_F(SplitViewHighlightViewTest, LandscapeBoundsInRtl) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("he");

  const gfx::Rect bounds(0, 0, 100, 100);
  left_highlight()->SetBounds(bounds, /*landscape=*/true,
                              /*animation_type=*/base::nullopt);

  // Tests that setting bounds without animations in landscape mode will set the
  // bounds of the components correctly, without any transforms. In rtl, the
  // bounds of the outer components are swapped.
  SplitViewHighlightViewTestApi test_api(left_highlight());
  EXPECT_EQ(gfx::Rect(86, 0, 14, 100), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(4, 0, 92, 100), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 14, 100), test_api.GetRightBottomView()->bounds());
  EXPECT_TRUE(GetTransform(test_api.GetLeftTopView()).IsIdentity());
  EXPECT_TRUE(GetTransform(test_api.GetMiddleView()).IsIdentity());
  EXPECT_TRUE(GetTransform(test_api.GetRightBottomView()).IsIdentity());

  // Tests that after animating to new bounds, the components have the same
  // bounds, but have transforms. In rtl the beginning element is the one that
  // is translated instead. The middle element has a extra translation in its
  // transform to account for the flipped scaling.
  const gfx::Rect new_bounds(0, 0, 200, 100);
  left_highlight()->SetBounds(
      new_bounds, /*landscape=*/true, /*animation_type=*/
      base::make_optional(SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(gfx::Rect(86, 0, 14, 100), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(4, 0, 92, 100), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 14, 100), test_api.GetRightBottomView()->bounds());
  gfx::Transform expected_begin_transform;
  expected_begin_transform.Translate(-100, 0);
  EXPECT_TRUE(GetTransform(test_api.GetLeftTopView())
                  .ApproximatelyEqual(expected_begin_transform));
  gfx::Transform expected_middle_transform;
  expected_middle_transform.Translate(-100, 0);
  expected_middle_transform.Scale(2.16, 1);
  EXPECT_TRUE(GetTransform(test_api.GetMiddleView())
                  .ApproximatelyEqual(expected_middle_transform));
  EXPECT_TRUE(GetTransform(test_api.GetRightBottomView()).IsIdentity());
}

class SplitViewHighlightViewPortraitTest
    : public SplitViewHighlightViewTest,
      public testing::WithParamInterface<bool> {
 public:
  SplitViewHighlightViewPortraitTest()
      : scoped_locale_(GetParam() ? "he" : "") {}
  ~SplitViewHighlightViewPortraitTest() override = default;

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewHighlightViewPortraitTest);
};

// Tests setting and animating bounds for the split view highlight view in
// portrait mode. The bounds should remain the same in ltr or rtl.
TEST_P(SplitViewHighlightViewPortraitTest, Bounds) {
  const gfx::Rect bounds(0, 0, 100, 100);
  left_highlight()->SetBounds(bounds, /*landscape=*/false,
                              /*animation_type=*/base::nullopt);

  SplitViewHighlightViewTestApi test_api(left_highlight());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 14), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(0, 4, 100, 92), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(0, 86, 100, 14), test_api.GetRightBottomView()->bounds());

  const gfx::Rect new_bounds(0, 0, 100, 200);
  left_highlight()->SetBounds(
      new_bounds, /*landscape=*/false, /*animation_type=*/
      base::make_optional(SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(gfx::Rect(0, 0, 100, 14), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(0, 4, 100, 92), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(0, 86, 100, 14), test_api.GetRightBottomView()->bounds());
  EXPECT_TRUE(GetTransform(test_api.GetLeftTopView()).IsIdentity());
  gfx::Transform expected_middle_transform;
  expected_middle_transform.Scale(1, 2.16);
  EXPECT_TRUE(GetTransform(test_api.GetMiddleView())
                  .ApproximatelyEqual(expected_middle_transform));
  gfx::Transform expected_end_transform;
  expected_end_transform.Translate(0, 100);
  EXPECT_TRUE(GetTransform(test_api.GetRightBottomView())
                  .ApproximatelyEqual(expected_end_transform));
}

INSTANTIATE_TEST_SUITE_P(Bounds,
                         SplitViewHighlightViewPortraitTest,
                         testing::Bool());

TEST_F(SplitViewHighlightViewTest, RightBounds) {
  const gfx::Rect bounds(100, 0, 100, 100);
  right_highlight()->SetBounds(bounds, /*landscape=*/true,
                               /*animation_type=*/base::nullopt);

  SplitViewHighlightViewTestApi test_api(right_highlight());
  EXPECT_EQ(gfx::Rect(0, 0, 14, 100), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(4, 0, 92, 100), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(86, 0, 14, 100), test_api.GetRightBottomView()->bounds());

  const gfx::Rect new_bounds(0, 0, 200, 100);
  right_highlight()->SetBounds(
      new_bounds, /*landscape=*/true, /*animation_type=*/
      base::make_optional(SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(gfx::Rect(100, 0, 14, 100), test_api.GetLeftTopView()->bounds());
  EXPECT_EQ(gfx::Rect(104, 0, 92, 100), test_api.GetMiddleView()->bounds());
  EXPECT_EQ(gfx::Rect(186, 0, 14, 100),
            test_api.GetRightBottomView()->bounds());
  gfx::Transform expected_begin_transform;
  expected_begin_transform.Translate(-100, 0);
  EXPECT_TRUE(GetTransform(test_api.GetLeftTopView())
                  .ApproximatelyEqual(expected_begin_transform));
  gfx::Transform expected_middle_transform;
  expected_middle_transform.Translate(-100, 0);
  expected_middle_transform.Scale(2.16, 1);
  EXPECT_TRUE(GetTransform(test_api.GetMiddleView())
                  .ApproximatelyEqual(expected_middle_transform));
  EXPECT_TRUE(GetTransform(test_api.GetRightBottomView()).IsIdentity());
}

}  // namespace ash

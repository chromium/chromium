// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr int kLongerEdge = kPreferredWidthDip + 10;
constexpr int kShorterEdge = kPreferredWidthDip - 10;

class AssistantOptInViewUnittest : public AssistantAshTestBase {};

// If screen rotation happens with small screen (an edge is smaller than
// |kPreferredWidthDip|), relayout of styled label will happen, i.e.
// |AssistantPageView| doesn't get bigger than |kPreferredWidthDip|. This test
// case makes sure that styled label is relayouted correctly.
TEST_F(AssistantOptInViewUnittest, RotateSmallScreen) {
  UpdateDisplay(base::StringPrintf("%dx%d", kLongerEdge, kShorterEdge));
  // Test in tablet mode because the launcher in clamshell mode doesn't resize
  // lower than 640 dips wide.
  SetTabletMode(true);
  ShowAssistantUi();

  const views::View* styled_label =
      opt_in_view()->GetViewByID(AssistantViewID::kOptInViewStyledLabel);
  ASSERT_THAT(styled_label, testing::NotNull());

  // Assert that bounds of |opt_in_view()| contains it of |styled_label|. We
  // check it by converting bounds of |styled_label| to the coordinate of
  // |opt_in_view()|'s parent as bounds of |opt_in_view()| itself is in its
  // parent coordinate.
  gfx::RectF styled_label_bounds(styled_label->bounds());
  views::View::ConvertRectToTarget(styled_label, opt_in_view()->parent(),
                                   &styled_label_bounds);
  ASSERT_TRUE(opt_in_view()->bounds().Contains(
      gfx::ToEnclosingRect(styled_label_bounds)));

  // Rotate display 90 degree by changing bounds.
  int original_width = opt_in_view()->bounds().width();
  UpdateDisplay(base::StringPrintf("%dx%d", kShorterEdge, kLongerEdge));

  // Assert that relayout of |opt_in_view| is necessary.
  ASSERT_THAT(opt_in_view()->bounds().width(), testing::Ne(original_width));

  // Confirm that |opt_in_view()| contains |styled_label|.
  styled_label_bounds = gfx::RectF(styled_label->bounds());
  views::View::ConvertRectToTarget(styled_label, opt_in_view()->parent(),
                                   &styled_label_bounds);
  EXPECT_TRUE(opt_in_view()->bounds().Contains(
      gfx::ToEnclosingRect(styled_label_bounds)));
}

}  // namespace
}  // namespace ash

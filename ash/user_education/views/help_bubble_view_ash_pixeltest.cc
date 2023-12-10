// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "ash/user_education/views/help_bubble_view_ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_education/common/help_bubble_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using user_education::HelpBubbleArrow;

// Helpers ---------------------------------------------------------------------

void EmplaceBackIf(std::vector<std::string>& container,
                   std::string value,
                   bool condition) {
  if (condition) {
    container.emplace_back(std::move(value));
  }
}

}  // namespace

// HelpBubbleViewAshPixelTestBase ----------------------------------------------

// Base class for pixel tests of `HelpBubbleViewAsh`.
class HelpBubbleViewAshPixelTestBase : public HelpBubbleViewAshTestBase {
 public:
  HelpBubbleViewAshPixelTestBase() {
    // Features using help bubble views are not launching until post-Jelly, so
    // ensure that benchmark images are taken with the Jelly flag enabled.
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kJelly);
  }

 private:
  // HelpBubbleViewAshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // Used to enable the Jelly flag so that benchmark images accurately reflect
  // the state of the world when features using help bubble views launch.
  base::test::ScopedFeatureList scoped_feature_list_;
};

// HelpBubbleViewPixelTest -----------------------------------------------------

// Base class for pixel tests of `HelpBubbleViewAsh` parameterized by attributes
// to include when creating help bubble views.
class HelpBubbleViewAshPixelTest
    : public HelpBubbleViewAshPixelTestBase,
      public testing::WithParamInterface<std::tuple<
          /*with_title_text=*/bool,
          /*with_body_icon=*/bool,
          /*with_buttons=*/bool,
          /*with_progress=*/bool>> {
 public:
  // Returns whether an attribute should be included when creating help bubble
  // views given test parameterization.
  bool with_title_text() const { return std::get<0>(GetParam()); }
  bool with_body_icon() const { return std::get<1>(GetParam()); }
  bool with_buttons() const { return std::get<2>(GetParam()); }
  bool with_progress() const { return std::get<3>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HelpBubbleViewAshPixelTest,
    testing::Combine(
        /*with_title_text=*/testing::Bool(),
        /*with_body_icon=*/testing::Bool(),
        /*with_buttons=*/testing::Bool(),
        /*with_progress=*/testing::Bool()),
    [](const auto& info) {
      std::vector<std::string> param_name{"HelpBubbleViewAsh"};
      EmplaceBackIf(param_name, "WithTitleText", std::get<0>(info.param));
      EmplaceBackIf(param_name, "WithBodyIcon", std::get<1>(info.param));
      EmplaceBackIf(param_name, "WithButtons", std::get<2>(info.param));
      EmplaceBackIf(param_name, "WithProgress", std::get<3>(info.param));
      return base::JoinString(param_name, "_");
    });

// Tests -----------------------------------------------------------------------

// Protects against regression in `HelpBubbleViewAsh` appearance by comparing
// help bubble views created with attributes according to test parameterization
// against benchmark images.
TEST_P(HelpBubbleViewAshPixelTest, Appearance) {
  auto* help_bubble_view =
      CreateHelpBubbleView(HelpBubbleArrow::kNone, with_title_text(),
                           with_body_icon(), with_buttons(), with_progress());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "appearance", /*revision_number=*/7, help_bubble_view,
      help_bubble_view->anchor_widget()));
}

// HelpBubbleViewAshArrowPixelTest ---------------------------------------------

// Base class for pixel tests of `HelpBubbleArrow` as it relates to
// `HelpBubbleViewAsh`, parameterized by arrow to use when creating help bubble
// views.
class HelpBubbleViewAshArrowPixelTest
    : public HelpBubbleViewAshPixelTestBase,
      public testing::WithParamInterface<HelpBubbleArrow> {
 public:
  // Returns the arrow to use when creating help bubble views given test
  // parameterization.
  HelpBubbleArrow arrow() const { return GetParam(); }
};

#define ENUM_CASE(Enum, Value) \
  case Enum::Value:            \
    return #Enum "_" #Value;

INSTANTIATE_TEST_SUITE_P(All,
                         HelpBubbleViewAshArrowPixelTest,
                         testing::Values(HelpBubbleArrow::kNone,
                                         HelpBubbleArrow::kTopLeft,
                                         HelpBubbleArrow::kTopRight,
                                         HelpBubbleArrow::kBottomLeft,
                                         HelpBubbleArrow::kBottomRight,
                                         HelpBubbleArrow::kLeftTop,
                                         HelpBubbleArrow::kRightTop,
                                         HelpBubbleArrow::kLeftBottom,
                                         HelpBubbleArrow::kRightBottom,
                                         HelpBubbleArrow::kTopCenter,
                                         HelpBubbleArrow::kBottomCenter,
                                         HelpBubbleArrow::kLeftCenter,
                                         HelpBubbleArrow::kRightCenter),
                         [](const auto& info) {
                           switch (info.param) {
                             ENUM_CASE(HelpBubbleArrow, kNone);
                             ENUM_CASE(HelpBubbleArrow, kTopLeft);
                             ENUM_CASE(HelpBubbleArrow, kTopRight);
                             ENUM_CASE(HelpBubbleArrow, kBottomLeft);
                             ENUM_CASE(HelpBubbleArrow, kBottomRight);
                             ENUM_CASE(HelpBubbleArrow, kLeftTop);
                             ENUM_CASE(HelpBubbleArrow, kRightTop);
                             ENUM_CASE(HelpBubbleArrow, kLeftBottom);
                             ENUM_CASE(HelpBubbleArrow, kRightBottom);
                             ENUM_CASE(HelpBubbleArrow, kTopCenter);
                             ENUM_CASE(HelpBubbleArrow, kBottomCenter);
                             ENUM_CASE(HelpBubbleArrow, kLeftCenter);
                             ENUM_CASE(HelpBubbleArrow, kRightCenter);
                           }
                         });

// Tests -----------------------------------------------------------------------

// Protects against regression in `HelpBubbleViewAsh` placement by comparing
// help bubble views created with arrows according to test parameterization
// against benchmark images.
TEST_P(HelpBubbleViewAshArrowPixelTest, Placement) {
  auto* help_bubble_view = CreateHelpBubbleView(
      arrow(), /*with_title_text=*/true, /*with_body_icon=*/true,
      /*with_buttons=*/true, /*with_progress=*/true);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "placement", /*revision_number=*/7, help_bubble_view,
      help_bubble_view->anchor_widget()));
}

}  // namespace ash

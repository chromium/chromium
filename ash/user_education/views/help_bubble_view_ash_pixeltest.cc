// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/views/help_bubble_view_ash.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using user_education::HelpBubbleArrow;
using user_education::HelpBubbleButtonParams;
using user_education::HelpBubbleParams;

// Helpers ---------------------------------------------------------------------

void EmplaceBackIf(std::vector<std::string>& container,
                   std::string value,
                   bool condition) {
  if (condition) {
    container.emplace_back(std::move(value));
  }
}

std::u16string Repeat(base::StringPiece16 str, size_t times) {
  std::vector<base::StringPiece16> strs(times);
  base::ranges::fill(strs, str);
  return base::JoinString(strs, u" ");
}

}  // namespace

// HelpBubbleViewAshPixelTestBase ----------------------------------------------

// Base class for pixel tests of `HelpBubbleViewAsh`.
class HelpBubbleViewAshPixelTestBase : public AshTestBase {
 public:
  HelpBubbleViewAshPixelTestBase() {
    // Features using help bubble views are not launching until post-Jelly, so
    // ensure that benchmark images are taken with the Jelly flag enabled.
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kJelly);
  }

  // Creates and returns a pointer to a new `HelpBubbleViewAsh` instance with
  // the specified attributes. Note that the returned help bubble view is owned
  // by its widget.
  HelpBubbleViewAsh* CreateHelpBubbleView(HelpBubbleArrow arrow,
                                          bool with_title_text,
                                          bool with_body_icon,
                                          bool with_buttons,
                                          bool with_progress) {
    HelpBubbleParams params;
    params.arrow = arrow;

    // NOTE: `HelpBubbleViewAsh` will never be created without body text.
    params.body_text = Repeat(u"Body", /*times=*/50);

    if (with_title_text) {
      params.title_text = Repeat(u"Title", /*times=*/25u);
    }

    if (with_body_icon) {
      params.body_icon = &vector_icons::kCelebrationIcon;
    }

    if (with_buttons) {
      HelpBubbleButtonParams button_params;
      button_params.text = u"Primary";
      button_params.is_default = true;
      params.buttons.emplace_back(std::move(button_params));

      button_params.text = u"Secondary";
      button_params.is_default = false;
      params.buttons.emplace_back(std::move(button_params));
    }

    if (with_progress) {
      params.progress = std::make_pair(2, 3);
    }

    // Anchor the help bubble view to the test `widget_`.
    internal::HelpBubbleAnchorParams anchor_params;
    anchor_params.view = widget_->GetContentsView();
    anchor_params.show_arrow = false;

    // NOTE: The returned help bubble view is owned by its widget.
    return new HelpBubbleViewAsh(anchor_params, std::move(params));
  }

 private:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Use a slightly larger display than is default to ensure that help bubble
    // views are fully on screen in all test scenarios.
    UpdateDisplay("1024x768");

    // Initialize a test `widget_` to be used as an anchor for help bubble
    // views. Note that shadow is removed since pixel tests of help bubble views
    // should not fail solely due to changes in shadow appearance of the anchor.
    views::Widget::InitParams params;
    params.layer_type = ui::LAYER_SOLID_COLOR;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(params));

    // Give the `widget_` color so that it stands out in benchmark images.
    widget_->GetLayer()->SetColor(gfx::kPlaceholderColor);

    // Center the `widget_` so that we can confirm various anchoring strategies
    // are working as intended.
    widget_->CenterWindow(gfx::Size(50, 50));
    widget_->ShowInactive();
  }

  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // Used to enable the Jelly flag so that benchmark images accurately reflect
  // the state of the world when features using help bubble views launch.
  base::test::ScopedFeatureList scoped_feature_list_;

  // The test `widget_` to be used as an anchor for help bubble views.
  views::UniqueWidgetPtr widget_;
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
      "appearance", /*revision_number=*/1u, help_bubble_view,
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
      "placement", /*revision_number=*/1u, help_bubble_view,
      help_bubble_view->anchor_widget()));
}

}  // namespace ash

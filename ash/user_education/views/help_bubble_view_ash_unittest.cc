// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/views/help_bubble_view_ash.h"

#include <vector>

#include "ash/user_education/user_education_types.h"
#include "ash/user_education/views/help_bubble_view_ash_test_base.h"
#include "components/user_education/common/help_bubble_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"

namespace ash {
namespace {

// Aliases.
using ::testing::Conditional;
using ::testing::Eq;
using ::user_education::HelpBubbleArrow;

// Helpers ---------------------------------------------------------------------

std::vector<absl::optional<HelpBubbleStyle>> GetHelpBubbleStyles() {
  std::vector<absl::optional<HelpBubbleStyle>> styles;
  styles.emplace_back(absl::nullopt);
  for (size_t i = static_cast<size_t>(HelpBubbleStyle::kMinValue);
       i <= static_cast<size_t>(HelpBubbleStyle::kMaxValue); ++i) {
    styles.emplace_back(static_cast<HelpBubbleStyle>(i));
  }
  return styles;
}

}  // namespace

// HelpBubbleViewAshStyleTest --------------------------------------------------

// Base class for tests of `HelpBubbleViewAsh` parameterized by style.
class HelpBubbleViewAshStyleTest
    : public HelpBubbleViewAshTestBase,
      public ::testing::WithParamInterface<absl::optional<HelpBubbleStyle>> {
 public:
  // Returns the help bubble style to use given test parameterization.
  const absl::optional<HelpBubbleStyle>& style() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HelpBubbleViewAshStyleTest,
                         ::testing::ValuesIn(GetHelpBubbleStyles()));

// Tests -----------------------------------------------------------------------

// Verifies that help bubbles have the appropriate background color given style.
TEST_P(HelpBubbleViewAshStyleTest, BackgroundColor) {
  const auto* const help_bubble_view = CreateHelpBubbleView(style());
  const auto* const color_provider = help_bubble_view->GetColorProvider();
  EXPECT_THAT(
      help_bubble_view->color(),
      Conditional(
          help_bubble_view->style() == HelpBubbleStyle::kDialog,
          Eq(color_provider->GetColor(cros_tokens::kCrosSysDialogContainer)),
          Eq(color_provider->GetColor(cros_tokens::kCrosSysBaseElevated))));
}

// Verifies that help bubbles can activate so long as they are not nudge style.
TEST_P(HelpBubbleViewAshStyleTest, CanActivate) {
  const auto* const help_bubble_view = CreateHelpBubbleView(style());
  EXPECT_EQ(help_bubble_view->CanActivate(),
            help_bubble_view->style() != HelpBubbleStyle::kNudge);
}

// Verifies that style is propagated to the help bubble as expected. Note that
// if not explicitly provided, style defaults to `HelpBubbleStyle::kDialog`.
TEST_P(HelpBubbleViewAshStyleTest, Style) {
  const auto* const help_bubble_view = CreateHelpBubbleView(style());
  EXPECT_EQ(help_bubble_view->style(),
            style().value_or(HelpBubbleStyle::kDialog));
}

}  // namespace ash

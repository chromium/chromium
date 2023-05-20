// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_ASH_TEST_BASE_H_
#define ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_ASH_TEST_BASE_H_

#include "ash/test/ash_test_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace user_education {
enum class HelpBubbleArrow;
struct HelpBubbleParams;
}  // namespace user_education

namespace ash {

class HelpBubbleViewAsh;
enum class HelpBubbleStyle;

// Base class for tests of `HelpBubbleViewAsh`.
class HelpBubbleViewAshTestBase : public AshTestBase {
 public:
  // Creates and returns a pointer to a new `HelpBubbleViewAsh` instance with
  // the specified attributes. Note that the returned help bubble view is owned
  // by its widget.
  HelpBubbleViewAsh* CreateHelpBubbleView(user_education::HelpBubbleArrow arrow,
                                          bool with_title_text,
                                          bool with_body_icon,
                                          bool with_buttons,
                                          bool with_progress);

  // Creates and returns a pointer to a new `HelpBubbleViewAsh` instance with
  // the specified `style`. Note that the returned help bubble view is owned
  // by its widget.
  HelpBubbleViewAsh* CreateHelpBubbleView(
      const absl::optional<HelpBubbleStyle>& style);

 private:
  // AshTestBase:
  void SetUp() override;

  // Creates and returns a pointer to a new `HelpBubbleViewAsh` instance with
  // the specified `params`. Note that the returned help bubble view is owned
  // by its widget.
  HelpBubbleViewAsh* CreateHelpBubbleView(
      user_education::HelpBubbleParams params);

  // The test `widget_` to be used as an anchor for help bubble views.
  views::UniqueWidgetPtr widget_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_ASH_TEST_BASE_H_

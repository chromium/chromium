// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/views/help_bubble_view_ash_test_base.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using user_education::HelpBubbleArrow;
using user_education::HelpBubbleButtonParams;
using user_education::HelpBubbleParams;

// Helpers ---------------------------------------------------------------------

std::u16string Repeat(std::u16string_view str, size_t times) {
  std::vector<std::u16string_view> strs(times);
  base::ranges::fill(strs, str);
  return base::JoinString(strs, u" ");
}

}  // namespace

// HelpBubbleViewAshTestBase ---------------------------------------------------

HelpBubbleViewAsh* HelpBubbleViewAshTestBase::CreateHelpBubbleView() {
  HelpBubbleParams params;
  params.arrow = HelpBubbleArrow::kNone;

  // NOTE: The returned help bubble view is owned by its widget.
  return CreateHelpBubbleView(std::move(params));
}

HelpBubbleViewAsh* HelpBubbleViewAshTestBase::CreateHelpBubbleView(
    HelpBubbleArrow arrow,
    bool with_title_text,
    bool with_body_icon,
    bool with_buttons,
    bool with_progress) {
  HelpBubbleParams params;
  params.arrow = arrow;

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

  // NOTE: The returned help bubble view is owned by its widget.
  return CreateHelpBubbleView(std::move(params));
}

HelpBubbleViewAsh* HelpBubbleViewAshTestBase::CreateHelpBubbleView(
    HelpBubbleParams params) {
  // NOTE: `HelpBubbleViewAsh` will never be created without body text.
  params.body_text = Repeat(u"Body", /*times=*/50);

  // Anchor the help bubble view to the test `widget_`.
  internal::HelpBubbleAnchorParams anchor_params;
  anchor_params.view = widget_->GetContentsView();

  // NOTE: The returned help bubble view is owned by its widget.
  return new HelpBubbleViewAsh(HelpBubbleId::kTest, anchor_params,
                               std::move(params));
}

void HelpBubbleViewAshTestBase::SetUp() {
  AshTestBase::SetUp();

  // Use a slightly larger display than is default to ensure that help bubble
  // views are fully on screen in all test scenarios.
  UpdateDisplay("1366x1080");

  // Initialize a test `widget_` to be used as an anchor for help bubble
  // views. Note that shadow is removed since pixel tests of help bubble views
  // should not fail solely due to changes in shadow appearance of the anchor.
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
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

}  // namespace ash

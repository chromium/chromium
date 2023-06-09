// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_nudge_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

namespace {

// Nudge constants
constexpr gfx::Insets kNudgeInteriorMargin = gfx::Insets::VH(20, 24);
constexpr gfx::Insets kTextOnlyNudgeInteriorMargin = gfx::Insets::VH(12, 20);
constexpr float kNudgeCornerRadius = 24.0f;

// Label constants
constexpr int kLabelMaxWidth_TextOnlyNudge = 300;
constexpr int kLabelMaxWidth_NudgeWithoutLeadingImage = 292;
constexpr int kLabelMaxWidth_NudgeWithLeadingImage = 276;

// Image constants
constexpr int kImageViewSize = 64;
constexpr int kImageViewCornerRadius = 12;

// Button constants
constexpr gfx::Insets kButtonsMargins = gfx::Insets::VH(0, 8);

// Padding constants
constexpr int kButtonContainerTopPadding = 16;
constexpr int kImageViewTrailingPadding = 20;
constexpr int kTitleBottomPadding = 8;

void AddPaddingView(views::View* parent, int width, int height) {
  parent->AddChildView(std::make_unique<views::View>())
      ->SetPreferredSize(gfx::Size(width, height));
}

void SetupViewCornerRadius(views::View* view, int corner_radius) {
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  view->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
}

}  // namespace

SystemNudgeView::SystemNudgeView(const AnchoredNudgeData& nudge_data) {
  DCHECK(features::IsSystemNudgeV2Enabled());

  SetupViewCornerRadius(this, kNudgeCornerRadius);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kNudgeCornerRadius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderOnShadow
          : views::HighlightBorder::Type::kHighlightBorder1));

  SetOrientation(views::LayoutOrientation::kVertical);
  SetInteriorMargin(kNudgeInteriorMargin);

  auto* image_and_text_container =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kHorizontal)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                       .Build());

  if (!nudge_data.image_model.IsEmpty()) {
    image_view_ = image_and_text_container->AddChildView(
        views::Builder<views::ImageView>()
            .SetPreferredSize(gfx::Size(kImageViewSize, kImageViewSize))
            .SetImage(nudge_data.image_model)
            .Build());
    SetupViewCornerRadius(image_view_, kImageViewCornerRadius);

    AddPaddingView(image_and_text_container, kImageViewTrailingPadding,
                   kImageViewSize);
  }

  auto* text_container = image_and_text_container->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .Build());

  if (!nudge_data.title_text.empty()) {
    title_label_ = text_container->AddChildView(
        views::Builder<views::Label>()
            .SetText(nudge_data.title_text)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
            .SetAutoColorReadabilityEnabled(false)
            .SetSubpixelRenderingEnabled(false)
            .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                TypographyToken::kCrosTitle1))
            .Build());

    AddPaddingView(text_container, title_label_->width(), kTitleBottomPadding);
  }

  body_label_ = text_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(nudge_data.body_text)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetAutoColorReadabilityEnabled(false)
          .SetSubpixelRenderingEnabled(false)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosBody2))
          .SetMultiLine(true)
          .SetMaxLines(2)
          .Build());

  SetLabelsMaxWidth(nudge_data.image_model.IsEmpty()
                        ? kLabelMaxWidth_NudgeWithoutLeadingImage
                        : kLabelMaxWidth_NudgeWithLeadingImage);

  // Return early if there are no buttons.
  if (nudge_data.dismiss_text.empty()) {
    CHECK(nudge_data.second_button_text.empty());

    // Update nudge margins and labels max width if nudge only has text.
    if (nudge_data.title_text.empty() && nudge_data.image_model.IsEmpty()) {
      SetInteriorMargin(kTextOnlyNudgeInteriorMargin);
      SetLabelsMaxWidth(kLabelMaxWidth_TextOnlyNudge);
    }
    return;
  }

  // Add top padding for the buttons row.
  AddPaddingView(this, image_and_text_container->width(),
                 kButtonContainerTopPadding);

  auto* buttons_container =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                       .SetIgnoreDefaultMainAxisMargins(true)
                       .SetCollapseMargins(true)
                       .Build());
  buttons_container->SetDefault(views::kMarginsKey, kButtonsMargins);

  const bool has_second_button = !nudge_data.second_button_text.empty();

  dismiss_button_ = buttons_container->AddChildView(
      views::Builder<PillButton>()
          .SetCallback(std::move(nudge_data.dismiss_callback))
          .SetText(nudge_data.dismiss_text)
          .SetPillButtonType(has_second_button
                                 ? PillButton::Type::kSecondaryWithoutIcon
                                 : PillButton::Type::kPrimaryWithoutIcon)
          .SetFocusBehavior(views::View::FocusBehavior::ALWAYS)
          .Build());

  if (has_second_button) {
    second_button_ = buttons_container->AddChildView(
        views::Builder<PillButton>()
            .SetCallback(std::move(nudge_data.second_button_callback))
            .SetText(nudge_data.second_button_text)
            .SetPillButtonType(PillButton::Type::kPrimaryWithoutIcon)
            .SetFocusBehavior(views::View::FocusBehavior::ALWAYS)
            .Build());
  }
}

SystemNudgeView::~SystemNudgeView() = default;

void SystemNudgeView::SetLabelsMaxWidth(int max_width) {
  if (title_label_) {
    title_label_->SetMaximumWidthSingleLine(max_width);
  }
  body_label_->SetMaximumWidth(max_width);
}

BEGIN_METADATA(SystemNudgeView, views::View)
END_METADATA

}  // namespace ash

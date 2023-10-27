// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/system_nudge_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "ash/system/toast/nudge_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

namespace {

// Nudge constants
constexpr gfx::Insets kNudgeInteriorMargin = gfx::Insets::VH(20, 20);
constexpr gfx::Insets kTextOnlyNudgeInteriorMargin = gfx::Insets::VH(12, 20);

constexpr gfx::Insets kNudgeWithCloseButton_InteriorMargin =
    gfx::Insets::TLBR(8, 20, 20, 8);
constexpr gfx::Insets
    kNudgeWithCloseButton_ImageAndTextContainerInteriorMargin =
        gfx::Insets::TLBR(12, 0, 0, 12);
constexpr gfx::Insets kNudgeWithCloseButton_ButtonContainerInteriorMargin =
    gfx::Insets::TLBR(0, 0, 0, 12);

constexpr float kNudgeCornerRadius = 24.0f;

// Label constants
constexpr int kBodyLabelMaxLines = 3;

// Image constants
constexpr int kImageViewSize = 60;
constexpr int kImageViewCornerRadius = 12;

// Button constants
constexpr gfx::Insets kButtonsMargins = gfx::Insets::VH(0, 8);

// Padding constants
constexpr int kButtonContainerTopPadding = 16;
constexpr int kImageViewTrailingPadding = 16;
constexpr int kTitleBottomPadding = 4;

// Shadow constants
constexpr gfx::Point kShadowOrigin = gfx::Point(8, 8);

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

SystemNudgeView::SystemNudgeView(AnchoredNudgeData& nudge_data) {
  DCHECK(features::IsSystemNudgeV2Enabled());

  SetupViewCornerRadius(this, kNudgeCornerRadius);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kNudgeCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  SetNotifyEnterExitOnChild(true);

  // Since nudges have a large corner radius, we use the shadow on texture
  // layer. Refer to `ash::SystemShadowOnTextureLayer` for more details.
  shadow_ =
      SystemShadow::CreateShadowOnTextureLayer(SystemShadow::Type::kElevation4);
  shadow_->SetRoundedCornerRadius(kNudgeCornerRadius);

  SetOrientation(views::LayoutOrientation::kVertical);
  SetInteriorMargin(kNudgeInteriorMargin);
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  const bool nudge_is_text_only = nudge_data.image_model.IsEmpty() &&
                                  nudge_data.title_text.empty() &&
                                  nudge_data.first_button_text.empty();

  // Nudges without an anchor view that are not text-only will have a close
  // button that is visible on view hovered.
  const bool has_close_button =
      !nudge_data.is_anchored() && !nudge_is_text_only;

  views::View* image_and_text_container;
  auto image_and_text_container_unique =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetInteriorMargin(
              has_close_button
                  ? kNudgeWithCloseButton_ImageAndTextContainerInteriorMargin
                  : gfx::Insets())
          .Build();

  if (has_close_button) {
    SetInteriorMargin(kNudgeWithCloseButton_InteriorMargin);

    // Set the `image_and_text_container` parent to use a `FillLayout` so it can
    // allow for overlap with the close button.
    auto* fill_layout_container = AddChildView(std::make_unique<views::View>());
    fill_layout_container->SetLayoutManager(
        std::make_unique<views::FillLayout>());

    image_and_text_container = fill_layout_container->AddChildView(
        std::move(image_and_text_container_unique));

    auto* close_button_container = fill_layout_container->AddChildView(
        views::Builder<views::FlexLayoutView>()
            .SetOrientation(views::LayoutOrientation::kHorizontal)
            .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
            .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
            .Build());

    close_button_ = close_button_container->AddChildView(
        views::Builder<views::ImageButton>()
            .SetID(VIEW_ID_SYSTEM_NUDGE_CLOSE_BUTTON)
            .SetCallback(std::move(nudge_data.close_button_callback))
            .SetImageModel(views::Button::STATE_NORMAL,
                           ui::ImageModel::FromVectorIcon(
                               kCloseSmallIcon, cros_tokens::kCrosSysOnSurface))
            .SetTooltipText(l10n_util::GetStringUTF16(
                IDS_ASH_SYSTEM_NUDGE_CLOSE_BUTTON_TOOLTIP))
            .SetVisible(false)
            .Build());
  } else {
    image_and_text_container =
        AddChildView(std::move(image_and_text_container_unique));
  }

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

  auto label_width = nudge_data.image_model.IsEmpty()
                         ? kNudgeLabelWidth_NudgeWithoutLeadingImage
                         : kNudgeLabelWidth_NudgeWithLeadingImage;

  if (!nudge_data.title_text.empty()) {
    title_label_ = text_container->AddChildView(
        views::Builder<views::Label>()
            .SetText(nudge_data.title_text)
            .SetTooltipText(nudge_data.title_text)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
            .SetAutoColorReadabilityEnabled(false)
            .SetSubpixelRenderingEnabled(false)
            .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                TypographyToken::kCrosButton1))
            .SetMaximumWidthSingleLine(label_width)
            .Build());

    AddPaddingView(text_container, title_label_->width(), kTitleBottomPadding);
  }

  body_label_ = text_container->AddChildView(
      views::Builder<views::Label>()
          .SetText(nudge_data.body_text)
          .SetTooltipText(nudge_data.body_text)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetAutoColorReadabilityEnabled(false)
          .SetSubpixelRenderingEnabled(false)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation1))
          .SetMultiLine(true)
          .SetMaxLines(kBodyLabelMaxLines)
          .SizeToFit(label_width)
          .Build());

  // Return early if there are no buttons.
  if (nudge_data.first_button_text.empty()) {
    CHECK(nudge_data.second_button_text.empty());

    // Update nudge margins and body label max width if nudge only has text.
    if (nudge_is_text_only) {
      SetInteriorMargin(kTextOnlyNudgeInteriorMargin);
      // `SizeToFit` is reset to zero so a maximum width can be set.
      body_label_->SizeToFit(0);
      body_label_->SetMaximumWidth(kNudgeLabelWidth_TextOnlyNudge);
    }
    return;
  }

  // Add top padding for the buttons row.
  AddPaddingView(this, image_and_text_container->width(),
                 kButtonContainerTopPadding);

  auto* buttons_container = AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetInteriorMargin(
              has_close_button
                  ? kNudgeWithCloseButton_ButtonContainerInteriorMargin
                  : gfx::Insets())
          .SetIgnoreDefaultMainAxisMargins(true)
          .SetCollapseMargins(true)
          .Build());
  buttons_container->SetDefault(views::kMarginsKey, kButtonsMargins);

  const bool has_second_button = !nudge_data.second_button_text.empty();

  first_button_ = buttons_container->AddChildView(
      views::Builder<PillButton>()
          .SetCallback(std::move(nudge_data.first_button_callback))
          .SetText(nudge_data.first_button_text)
          .SetTooltipText(nudge_data.first_button_text)
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
            .SetTooltipText(nudge_data.second_button_text)
            .SetPillButtonType(PillButton::Type::kPrimaryWithoutIcon)
            .SetFocusBehavior(views::View::FocusBehavior::ALWAYS)
            .Build());
  }
}

SystemNudgeView::~SystemNudgeView() = default;

void SystemNudgeView::UpdateShadowBounds() {
  shadow_->SetContentBounds(gfx::Rect(kShadowOrigin, GetPreferredSize()));
}

void SystemNudgeView::AddedToWidget() {
  shadow_->SetContentBounds(gfx::Rect(kShadowOrigin, GetPreferredSize()));

  // Attach the shadow at the bottom of the widget layer.
  auto* shadow_layer = shadow_->GetLayer();
  auto* widget_layer = GetWidget()->GetLayer();

  widget_layer->Add(shadow_layer);
  widget_layer->StackAtBottom(shadow_layer);
}

void SystemNudgeView::OnMouseEntered(const ui::MouseEvent& event) {
  HandleOnMouseHovered(/*mouse_entered=*/true);
}

void SystemNudgeView::OnMouseExited(const ui::MouseEvent& event) {
  HandleOnMouseHovered(/*mouse_entered=*/false);
}

void SystemNudgeView::HandleOnMouseHovered(const bool mouse_entered) {
  if (!close_button_) {
    return;
  }

  close_button_->SetVisible(mouse_entered);
}

BEGIN_METADATA(SystemNudgeView, views::View)
END_METADATA

}  // namespace ash

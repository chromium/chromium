// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_inline_icon_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"

namespace ash {

namespace {

constexpr int kBorderThickness = 1;
constexpr float kFocusRingCornerRadius = 6.0f;
constexpr float kContentCornerRadius = 12.0f;
constexpr int kLeftRightMargin = 6;
constexpr int kIconSize = 14;
constexpr int kLabelMinEdgeLength = 20;

}  // namespace

SearchResultInlineIconView::SearchResultInlineIconView(
    bool use_modified_styling,
    bool is_first_key)
    : use_modified_styling_(use_modified_styling) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetProperty(views::kMarginsKey,
              gfx::Insets::TLBR(0, is_first_key ? 0 : kLeftRightMargin, 0,
                                kLeftRightMargin));
}

SearchResultInlineIconView::~SearchResultInlineIconView() = default;

void SearchResultInlineIconView::SetIcon(const gfx::VectorIcon& icon) {
  DCHECK(!label_);
  if (!icon_image_) {
    icon_image_ = AddChildView(std::make_unique<views::ImageView>());
    icon_image_->SetCanProcessEventsWithinSubtree(false);
    icon_image_->SetVisible(true);
  }

  icon_ = &icon;

  ui::ImageModel icon_model;
  if (ash::features::IsSearchCustomizableShortcutsInLauncherEnabled()) {
    icon_model = ui::ImageModel::FromVectorIcon(
        *icon_, use_modified_styling_
                    ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface);

    icon_image_->SetBackground(views::CreateThemedRoundedRectBackground(
        use_modified_styling_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                              : cros_tokens::kCrosSysSurface,
        kContentCornerRadius));
  } else {
    icon_model =
        ui::ImageModel::FromVectorIcon(*icon_, cros_tokens::kColorProminent);
  }
  icon_image_->SetImage(icon_model);
  icon_image_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  icon_image_->SetVisible(true);

  int icon_top_bottom_margin = (kLabelMinEdgeLength - kIconSize) / 2;
  icon_image_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(icon_top_bottom_margin, kLeftRightMargin,
                        icon_top_bottom_margin, kLeftRightMargin)));
  SetVisible(true);
}

void SearchResultInlineIconView::SetText(const std::u16string& text) {
  DCHECK(!icon_image_);
  if (!label_) {
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetVisible(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW_INLINE_ANSWER_DETAILS);
    label_->SetTextStyle(views::style::STYLE_EMPHASIZED);
    label_->SetAutoColorReadabilityEnabled(false);
  }

  label_->SetText(text);
  label_->SetVisible(true);

  if (ash::features::IsSearchCustomizableShortcutsInLauncherEnabled()) {
    label_->SetEnabledColorId(
        use_modified_styling_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                              : cros_tokens::kCrosSysOnSurface);
    label_->SetBackground(views::CreateThemedRoundedRectBackground(
        use_modified_styling_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                              : cros_tokens::kCrosSysSurface,
        kContentCornerRadius));
  } else {
    label_->SetEnabledColorId(cros_tokens::kCrosSysPrimary);
  }

  int label_left_right_margin =
      std::max(kLeftRightMargin, (kLabelMinEdgeLength - label_->width()) / 2);
  label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, label_left_right_margin, 0, label_left_right_margin)));

  SetVisible(true);
}

void SearchResultInlineIconView::SetTooltipTextForImageView(
    const std::u16string& text) {
  icon_image_->SetTooltipText(text);
}

void SearchResultInlineIconView::OnPaint(gfx::Canvas* canvas) {
  if (ash::features::IsSearchCustomizableShortcutsInLauncherEnabled()) {
    return;
  }

  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysPrimary));
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeWidth(kBorderThickness);
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(gfx::Insets(kBorderThickness));
  canvas->DrawRoundRect(bounds, kFocusRingCornerRadius, paint_flags);
}

BEGIN_METADATA(SearchResultInlineIconView)
END_METADATA

}  // namespace ash

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_inline_icon_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
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
constexpr float kButtonCornerRadius = 6.0f;
constexpr int kLeftRightMargin = 6;
constexpr int kIconSize = 14;
constexpr int kLabelMinEdgeLength = 20;

}  // namespace

SearchResultInlineIconView::SearchResultInlineIconView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
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
  icon_image_->SetImage(gfx::CreateVectorIcon(
      *icon_, AshColorProvider::Get()->GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kTextColorURL)));
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
    label_->SetBackgroundColor(SK_ColorTRANSPARENT);
    label_->SetVisible(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW_INLINE_ANSWER_DETAILS);
    label_->SetTextStyle(views::style::STYLE_EMPHASIZED);
  }

  label_->SetText(text);
  label_->SetVisible(true);

  int label_left_right_margin =
      std::max(kLeftRightMargin, (kLabelMinEdgeLength - label_->width()) / 2);
  label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, label_left_right_margin, 0, label_left_right_margin)));

  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorURL));
  SetVisible(true);
}

void SearchResultInlineIconView::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorURL));
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeWidth(kBorderThickness);
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(gfx::Insets(kBorderThickness));
  canvas->DrawRoundRect(bounds, kButtonCornerRadius, paint_flags);
}

void SearchResultInlineIconView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (icon_image_) {
    DCHECK(icon_);
    icon_image_->SetImage(gfx::CreateVectorIcon(
        *icon_, AshColorProvider::Get()->GetContentLayerColor(
                    AshColorProvider::ContentLayerType::kTextColorURL)));
  }
  if (label_) {
    label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorURL));
  }
}

}  // namespace ash

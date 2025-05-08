// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_modal_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kCornerRadius = 4;
constexpr int kHorizontalPadding = 12;
constexpr int kVerticalPadding = 10;
constexpr int kSpacingBetweenItems = 8;
constexpr int kIconSize = 16;

}  // namespace

namespace glic {
GlicModalView::GlicModalView(const ui::ColorProvider* color_provider,
                             const std::u16string& label_text,
                             base::RepeatingClosure close_callback)
    : close_callback_(std::move(close_callback)) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetInsideBorderInsets(gfx::Insets::VH(kVerticalPadding, kHorizontalPadding));
  SetBetweenChildSpacing(kSpacingBetweenItems);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(kColorGlicModalBackground), kCornerRadius, 0));
  label_ = AddChildView(std::make_unique<views::Label>(label_text));
  label_->SetMultiLine(true);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetEnabledColor(color_provider->GetColor(kColorGlicModalForeground));
  label_->SetBackgroundColor(
      color_provider->GetColor(kColorGlicModalBackground));
  label_->SetProperty(views::kMarginsKey, gfx::Insets::VH(kVerticalPadding, 0));
  SetFlexForView(label_, 1);
  SetBetweenChildSpacing(kSpacingBetweenItems);
  auto close_button = std::make_unique<views::ImageButton>(close_callback_);
  views::SetImageFromVectorIconWithColor(
      close_button.get(), vector_icons::kCloseIcon, kIconSize,
      color_provider->GetColor(kColorGlicModalForeground),
      color_provider->GetColor(kColorGlicModalForeground));
  close_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOAST_CLOSE_TOOLTIP));
  close_button_ = AddChildView(std::move(close_button));
}

GlicModalView::~GlicModalView() = default;

}  // namespace glic

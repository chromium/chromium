// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/key_item_view.h"

#include <memory>

#include "ash/style/ash_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr int kKeyItemPadding = 4;
constexpr gfx::Size kIconSize{26, 26};
constexpr int kKeyItemCornerRadius = 8;

}  // namespace

KeyItemView::KeyItemView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBorder(views::CreateEmptyBorder(kKeyItemPadding));
}

KeyItemView::~KeyItemView() = default;

void KeyItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateRoundedRectBackground(
      GetColorProvider()->GetColor(kColorAshShieldAndBase80),
      kKeyItemCornerRadius));
}

void KeyItemView::Layout() {
  const auto bounds = GetContentsBounds();
  if (icon_)
    icon_->SetBoundsRect(bounds);

  if (label_)
    label_->SetBoundsRect(bounds);
}

gfx::Size KeyItemView::CalculatePreferredSize() const {
  int width = 0;
  int height = 0;

  for (const auto* child : children()) {
    const auto child_size = child->GetPreferredSize();
    height = std::max(height, child_size.height());
    width += child_size.width();
  }
  const auto insets = GetInsets();
  return gfx::Size(width + insets.width(), height + insets.height());
}

void KeyItemView::SetIcon(const gfx::VectorIcon& icon) {
  if (!icon_) {
    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
    icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  }

  icon_->SetImage(
      ui::ImageModel::FromVectorIcon(icon, kColorAshButtonIconColor));
  icon_->SetImageSize(kIconSize);
}

void KeyItemView::SetText(const std::u16string& text) {
  if (!label_) {
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetEnabledColor(kColorAshTextColorPrimary);
    label_->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
    label_->SetFontList(views::Label::GetDefaultFontList().Derive(
        8, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  }
  label_->SetText(text);
}

BEGIN_METADATA(KeyItemView, views::View)
END_METADATA

}  // namespace ash

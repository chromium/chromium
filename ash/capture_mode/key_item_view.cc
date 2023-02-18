// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/key_item_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

constexpr int kKeyItemMinWidth = 56;
constexpr int kKeyItemHeight = 56;
constexpr int kKeyItemVerticalPadding = 16;
constexpr int kKeyItemHorizontalPadding = 20;
constexpr gfx::Size kIconSize{20, 20};
constexpr char kGoogleSansFont[] = "Google Sans";
constexpr int kKeyItemViewFontSize = 18;
constexpr int kKeyItemViewLineHeight = 24;

SkColor GetColor() {
  return capture_mode_util::GetColorProviderForNativeTheme()->GetColor(
      cros_tokens::kCrosSysSystemBaseElevated);
}

}  // namespace

KeyItemView::KeyItemView(ui::KeyboardCode key_code) : key_code_(key_code) {
  SetPaintToLayer();
  SetBackground(
      views::CreateRoundedRectBackground(GetColor(), kKeyItemHeight / 2));
  layer()->SetFillsBoundsOpaquely(false);
}

KeyItemView::~KeyItemView() = default;

void KeyItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  GetBackground()->SetNativeControlColor(GetColor());
  SetBorder(std::make_unique<views::HighlightBorder>(
      kKeyItemHeight / 2, views::HighlightBorder::Type::kHighlightBorder1,
      /*use_light_colors=*/false));
  SchedulePaint();
}

void KeyItemView::Layout() {
  const auto bounds = GetContentsBounds();
  if (icon_) {
    icon_->SetBoundsRect(bounds);
  }

  if (label_) {
    label_->SetBoundsRect(bounds);
  }
}

gfx::Size KeyItemView::CalculatePreferredSize() const {
  // Return the fixed size if the key item contains icon or label with a single
  // character.
  if (icon_ || (label_ && label_->GetText().length() == 1)) {
    return gfx::Size(kKeyItemMinWidth, kKeyItemHeight);
  }

  int width = 0;
  for (const auto* child : children()) {
    const auto child_size = child->GetPreferredSize();
    width += child_size.width();
  }

  width = std::max(width, kKeyItemMinWidth);
  return gfx::Size(width, kKeyItemHeight);
}

void KeyItemView::SetIcon(const gfx::VectorIcon& icon) {
  if (!icon_) {
    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
    icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  }

  icon_->SetImage(
      ui::ImageModel::FromVectorIcon(icon, cros_tokens::kCrosSysOnSurface));
  icon_->SetImageSize(kIconSize);
}

void KeyItemView::SetText(const std::u16string& text) {
  if (!label_) {
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    label_->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
    label_->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                      kKeyItemViewFontSize,
                                      gfx::Font::Weight::MEDIUM));
    label_->SetLineHeight(kKeyItemViewLineHeight);
  }

  // Set the border only when necessary which is the multi-character case and
  // clear the border for the single-character case in case the border was set
  // with the multi-character settings before.
  label_->SetBorder(
      text.length() == 1
          ? nullptr
          : views::CreateEmptyBorder(gfx::Insets::VH(
                kKeyItemVerticalPadding, kKeyItemHorizontalPadding)));
  label_->SetText(text);
}

BEGIN_METADATA(KeyItemView, views::View)
END_METADATA

}  // namespace ash

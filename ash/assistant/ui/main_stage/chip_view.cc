// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/chip_view.h"

#include <string>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {
namespace {
// Appearance.
constexpr int kStrokeWidthDip = 1;
constexpr int kFocusedStrokeWidthDip = 2;

constexpr int kIconMarginDip = 8;
constexpr int kChipPaddingDip = 16;
constexpr int kPreferredHeightDipDefault = 32;
constexpr int kPreferredHeightDipLarge = 36;
}  // namespace

ChipView::ChipView(Type type) : type_(type) {
  // Focus.
  // 1. Dark light mode is OFF
  // We change background color of a suggestion chip view. No focus ring is
  // used.
  // 2. Dark light mode is ON
  // We use focus ring. No background color change with focus.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);

  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetHaloThickness(kFocusedStrokeWidthDip);
  focus_ring->SetHaloInset(0.0f);

  // Path is used for the focus ring, i.e. path is not necessary for dark and
  // light mode flag off case. But we always install this as it shouldn't be a
  // problem even if we provide the path to the UI framework.
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, size());
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), radius);

  // Layout.
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, kChipPaddingDip, 0, kChipPaddingDip),
      kIconMarginDip));
  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Icon.
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon_view_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon_view_->SetVisible(false);

  // Text.
  text_view_ = AddChildView(std::make_unique<views::Label>());
  text_view_->SetID(kSuggestionChipViewLabel);
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetSubpixelRenderingEnabled(false);

  switch (type_) {
    case Type::kDefault: {
      const gfx::FontList& font_list = assistant::ui::GetDefaultFontList();
      text_view_->SetFontList(font_list.Derive(
          /*size_delta=*/1, font_list.GetFontStyle(),
          gfx::Font::Weight::MEDIUM));
      break;
    }
    case Type::kLarge: {
      raw_ptr<const TypographyProvider> typography_provider =
          TypographyProvider::Get();
      DCHECK(typography_provider) << "TypographyProvider must not be null";
      if (typography_provider) {
        typography_provider->StyleLabel(TypographyToken::kCrosButton1,
                                        *text_view_);
      }
    } break;
  }
}

ChipView::~ChipView() = default;

gfx::Size ChipView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_width =
      views::View::CalculatePreferredSize(available_size).width();
  return gfx::Size(preferred_width, type_ == Type::kDefault
                                        ? kPreferredHeightDipDefault
                                        : kPreferredHeightDipLarge);
}

void ChipView::ChildVisibilityChanged(views::View* child) {
  // When icon visibility is modified we need to update layout padding.
  if (child == icon_view_) {
    const int padding_left_dip =
        icon_view_->GetVisible() ? kIconMarginDip : kChipPaddingDip;
    layout_manager_->set_inside_border_insets(
        gfx::Insets::TLBR(0, padding_left_dip, 0, kChipPaddingDip));
  }
  PreferredSizeChanged();
}

bool ChipView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE) {
    return false;
  }
  return Button::OnKeyPressed(event);
}

void ChipView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  Button::OnBoundsChanged(previous_bounds);

  // If there is no change in height, we don't need to do anything as the code
  // below is to update corner radius values.
  if (height() == previous_bounds.height()) {
    return;
  }

  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, size());

  // Only set the border if a ColorProvider is available. Otherwise, we cannot
  // compute the stroke color.
  auto* color_provider = GetColorProvider();
  if (color_provider) {
    SetBorder(views::CreateRoundedRectBorder(
        kStrokeWidthDip, radius,
        color_provider->GetColor(kColorAshSeparatorColor)));
  }

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(), radius);
}

void ChipView::OnThemeChanged() {
  views::Button::OnThemeChanged();

  raw_ptr<ui::ColorProvider> color_provider = GetColorProvider();
  if (!color_provider) {
    DCHECK(false) << "ColorProvider should not be null";
    return;
  }

  text_view_->SetEnabledColor(
      color_provider->GetColor(kColorAshSuggestionChipViewTextView));
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, size());
  SetBorder(views::CreateRoundedRectBorder(
      kStrokeWidthDip, radius,
      color_provider->GetColor(kColorAshSeparatorColor)));
}

void ChipView::SetIcon(const gfx::ImageSkia& icon) {
  icon_view_->SetImage(icon);
  MakeIconVisible();
}

void ChipView::MakeIconVisible() {
  icon_view_->SetVisible(true);
}

gfx::ImageSkia ChipView::GetIcon() const {
  return icon_view_->GetImage();
}

void ChipView::SetText(const std::u16string& text) {
  text_view_->SetText(text);
  GetViewAccessibility().SetName(text);
}

const std::u16string& ChipView::GetText() const {
  return text_view_->GetText();
}

BEGIN_METADATA(ChipView)
END_METADATA

}  // namespace ash

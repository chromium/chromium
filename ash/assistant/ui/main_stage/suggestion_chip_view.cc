// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/assistant/util/resource_util.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
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

using assistant::util::ResourceLinkType;

// Appearance.
constexpr int kStrokeWidthDip = 1;
constexpr int kFocusedStrokeWidthDip = 2;

constexpr int kIconMarginDip = 8;
constexpr int kIconSizeDip = 16;
constexpr int kChipPaddingDip = 16;
constexpr int kPreferredHeightDip = 32;

}  // namespace

// SuggestionChipView ----------------------------------------------------------

SuggestionChipView::SuggestionChipView(AssistantViewDelegate* delegate,
                                       const AssistantSuggestion& suggestion)
    : delegate_(delegate), suggestion_id_(suggestion.id) {
  InitLayout(suggestion);
}

SuggestionChipView::~SuggestionChipView() = default;

gfx::Size SuggestionChipView::CalculatePreferredSize() const {
  const int preferred_width = views::View::CalculatePreferredSize().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int SuggestionChipView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionChipView::ChildVisibilityChanged(views::View* child) {
  // When icon visibility is modified we need to update layout padding.
  if (child == icon_view_) {
    const int padding_left_dip =
        icon_view_->GetVisible() ? kIconMarginDip : kChipPaddingDip;
    layout_manager_->set_inside_border_insets(
        gfx::Insets::TLBR(0, padding_left_dip, 0, kChipPaddingDip));
  }
  PreferredSizeChanged();
}

void SuggestionChipView::InitLayout(const AssistantSuggestion& suggestion) {
  const std::u16string text = base::UTF8ToUTF16(suggestion.text);

  // Accessibility.
  SetAccessibleName(text);

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
  // Note that padding differs depending on icon visibility.
  const int padding_left_dip =
      suggestion.icon_url.is_empty() ? kChipPaddingDip : kIconMarginDip;

  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, padding_left_dip, 0, kChipPaddingDip),
      kIconMarginDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Icon.
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon_view_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));

  const GURL& url = suggestion.icon_url;
  if (assistant::util::IsResourceLinkType(url, ResourceLinkType::kIcon)) {
    // Handle local images.
    icon_view_->SetImage(assistant::util::CreateVectorIcon(url, kIconSizeDip));
  } else if (!suggestion.icon_url.is_empty()) {
    // Handle remote images.
    delegate_->DownloadImage(url, base::BindOnce(&SuggestionChipView::SetIcon,
                                                 weak_factory_.GetWeakPtr()));
  } else {
    // Only hide the view if we have neither a local or a remote image. In the
    // case of a remote image, this prevents layout jank that would otherwise
    // occur if we updated the view visibility only after the image downloaded.
    icon_view_->SetVisible(false);
  }

  // Text.
  text_view_ = AddChildView(std::make_unique<views::Label>());
  text_view_->SetID(kSuggestionChipViewLabel);
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetSubpixelRenderingEnabled(false);
  const gfx::FontList& font_list = assistant::ui::GetDefaultFontList();
  text_view_->SetFontList(font_list.Derive(
      /*size_delta=*/1, font_list.GetFontStyle(), gfx::Font::Weight::MEDIUM));
  SetText(text);
}

bool SuggestionChipView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return false;
  return Button::OnKeyPressed(event);
}

void SuggestionChipView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  Button::OnBoundsChanged(previous_bounds);

  // If there is no change in height, we don't need to do anything as the code
  // below is to update corner radius values.
  if (height() == previous_bounds.height())
    return;

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

void SuggestionChipView::OnThemeChanged() {
  views::Button::OnThemeChanged();

  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);
  text_view_->SetEnabledColor(
      color_provider->GetColor(kColorAshSuggestionChipViewTextView));
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMaximum, size());
  SetBorder(views::CreateRoundedRectBorder(
      kStrokeWidthDip, radius,
      color_provider->GetColor(kColorAshSeparatorColor)));
}

void SuggestionChipView::SetIcon(const gfx::ImageSkia& icon) {
  icon_view_->SetImage(icon);
  icon_view_->SetVisible(!icon.isNull());
}

gfx::ImageSkia SuggestionChipView::GetIcon() const {
  return icon_view_->GetImage();
}

void SuggestionChipView::SetText(const std::u16string& text) {
  text_view_->SetText(text);
}

const std::u16string& SuggestionChipView::GetText() const {
  return text_view_->GetText();
}

BEGIN_METADATA(SuggestionChipView, views::Button)
END_METADATA

}  // namespace ash

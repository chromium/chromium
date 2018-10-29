// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/suggestion_chip_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

// Assistant specific style:
constexpr SkColor kAssistantBackgroundColor = SK_ColorWHITE;
constexpr SkColor kAssistantFocusColor = SkColorSetA(gfx::kGoogleGrey900, 0x14);
constexpr SkColor kAssistantStrokeColor =
    SkColorSetA(gfx::kGoogleGrey900, 0x24);
constexpr SkColor kAssistantTextColor = gfx::kGoogleGrey700;
constexpr int kAssistantStrokeWidthDip = 1;

// App list specific style:
constexpr SkColor kAppListBackgroundColor =
    SkColorSetA(gfx::kGoogleGrey900, 0x33);
constexpr SkColor kAppListTextColor = gfx::kGoogleGrey100;
constexpr SkColor kAppListRippleColor = SkColorSetA(gfx::kGoogleGrey100, 0x0F);
constexpr SkColor kAppListFocusColor = SkColorSetA(gfx::kGoogleGrey100, 0x14);
constexpr int kAppListMaxTextWidth = 192;
constexpr int kBlurRadius = 5;

// Shared style:
constexpr int kIconMarginDip = 8;
constexpr int kPaddingDip = 16;
constexpr int kPreferredHeightDip = 32;

}  // namespace

// Params ----------------------------------------------------------------------

SuggestionChipView::Params::Params() = default;

SuggestionChipView::Params::~Params() = default;

// SuggestionChipView ----------------------------------------------------------

SuggestionChipView::SuggestionChipView(const Params& params,
                                       views::ButtonListener* listener)
    : Button(listener),
      icon_view_(new views::ImageView()),
      text_view_(new views::Label()),
      assistant_style_(params.assistant_style) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInkDropMode(InkDropMode::ON);

  // Set background blur for the chip and use mask layer to clip it into
  // rounded rect.
  if (!assistant_style_)
    SetBackgroundBlurEnabled(false);

  InitLayout(params);
}

SuggestionChipView::~SuggestionChipView() = default;

void SuggestionChipView::SetBackgroundBlurEnabled(bool enabled) {
  DCHECK(!assistant_style_);

  // Background blur is enabled if and only if layer exists.
  if (!!layer() == enabled)
    return;

  if (!enabled) {
    DestroyLayer();
    return;
  }

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(kBlurRadius);
  SetRoundedRectMaskLayer(kPreferredHeightDip / 2);
}

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
        icon_view_->visible() ? kIconMarginDip : kPaddingDip;
    layout_manager_->set_inside_border_insets(
        gfx::Insets(0, padding_left_dip, 0, kPaddingDip));
  }
  PreferredSizeChanged();
}

void SuggestionChipView::InitLayout(const Params& params) {
  // Layout padding differs depending on icon visibility.
  const int padding_left_dip = params.icon ? kIconMarginDip : kPaddingDip;

  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, padding_left_dip, 0, kPaddingDip), kIconMarginDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Icon.
  const int icon_size =
      AppListConfig::instance().suggestion_chip_icon_dimension();
  icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  icon_view_->SetPreferredSize(gfx::Size(icon_size, icon_size));

  if (params.icon)
    icon_view_->SetImage(params.icon.value());
  else
    icon_view_->SetVisible(false);

  AddChildView(icon_view_);

  // Text.
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetEnabledColor(assistant_style_ ? kAssistantTextColor
                                               : kAppListTextColor);
  text_view_->SetSubpixelRenderingEnabled(false);
  text_view_->SetFontList(
      assistant_style_
          ? ash::assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(1)
          : AppListConfig::instance().app_title_font());
  SetText(params.text);
  AddChildView(text_view_);
}

void SuggestionChipView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  gfx::Rect bounds = GetContentsBounds();

  // Background.
  if (HasFocus()) {
    flags.setColor(assistant_style_ ? kAssistantFocusColor
                                    : kAppListFocusColor);
    canvas->DrawRoundRect(bounds, height() / 2, flags);
  } else {
    flags.setColor(assistant_style_ ? kAssistantBackgroundColor
                                    : kAppListBackgroundColor);
    canvas->DrawRoundRect(bounds, height() / 2, flags);
  }

  // Border.
  if (assistant_style_) {
    // Stroke should be drawn within our contents bounds.
    bounds.Inset(gfx::Insets(kAssistantStrokeWidthDip));

    // Stroke.
    flags.setColor(kAssistantStrokeColor);
    flags.setStrokeWidth(kAssistantStrokeWidthDip);
    flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
    canvas->DrawRoundRect(bounds, height() / 2, flags);
  }
}

void SuggestionChipView::OnFocus() {
  SchedulePaint();
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void SuggestionChipView::OnBlur() {
  SchedulePaint();
}

void SuggestionChipView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (chip_mask_)
    chip_mask_->layer()->SetBounds(GetContentsBounds());
}

std::unique_ptr<views::InkDrop> SuggestionChipView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(false);
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropMask> SuggestionChipView::CreateInkDropMask()
    const {
  return std::make_unique<views::RoundRectInkDropMask>(size(), gfx::InsetsF(),
                                                       height() / 2);
}

std::unique_ptr<views::InkDropRipple> SuggestionChipView::CreateInkDropRipple()
    const {
  const gfx::Point center = GetLocalBounds().CenterPoint();
  const int ripple_radius = width() / 2;
  gfx::Rect bounds(center.x() - ripple_radius, center.y() - ripple_radius,
                   2 * ripple_radius, 2 * ripple_radius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), kAppListRippleColor, 1.0f);
}

std::unique_ptr<ui::Layer> SuggestionChipView::RecreateLayer() {
  std::unique_ptr<ui::Layer> old_layer = views::View::RecreateLayer();
  if (layer())
    SetRoundedRectMaskLayer(kPreferredHeightDip / 2);
  return old_layer;
}

void SuggestionChipView::SetIcon(const gfx::ImageSkia& icon) {
  icon_view_->SetImage(icon);
  icon_view_->SetVisible(true);
}

void SuggestionChipView::SetText(const base::string16& text) {
  text_view_->SetText(text);
  if (!assistant_style_) {
    gfx::Size size = text_view_->CalculatePreferredSize();
    size.set_width(std::min(kAppListMaxTextWidth, size.width()));
    text_view_->SetPreferredSize(size);
  }
}

const base::string16& SuggestionChipView::GetText() const {
  return text_view_->text();
}

void SuggestionChipView::SetRoundedRectMaskLayer(int corner_radius) {
  chip_mask_ = views::Painter::CreatePaintedLayer(
      views::Painter::CreateSolidRoundRectPainter(SK_ColorBLACK,
                                                  corner_radius));
  chip_mask_->layer()->SetFillsBoundsOpaquely(false);
  chip_mask_->layer()->SetBounds(GetLocalBounds());
  layer()->SetMaskLayer(chip_mask_->layer());
}

}  // namespace app_list

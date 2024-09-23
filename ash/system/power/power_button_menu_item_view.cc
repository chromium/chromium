// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_item_view.h"

#include "ash/style/ash_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Size of the image icon in pixels.
constexpr int kIconSize = 24;

// Top padding of the image icon to the top of the item view.
constexpr int kIconTopPadding = 17;

// The distance from one line title's bottom to the top of the item view.
constexpr int kTitleTopPaddingIncludesOneLineHeight =
    kIconTopPadding + kIconSize + 22;

// The amount of rounding applied to the corners of the focused menu item.
constexpr int kFocusedItemRoundRectRadiusDp = 8;

// Line height of the label.
constexpr int kLineHeight = 20;

}  // namespace

PowerButtonMenuItemView::PowerButtonMenuItemView(
    PressedCallback callback,
    const gfx::VectorIcon& icon,
    const std::u16string& title_text)
    : views::ImageButton(std::move(callback)), icon_(icon) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  SetFocusPainter(nullptr);

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImage(
      ui::ImageModel::FromVectorIcon(*icon_, kColorAshIconColorPrimary));
  title_ = AddChildView(std::make_unique<views::Label>());
  title_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_->SetText(title_text);
  title_->SetVerticalAlignment(gfx::ALIGN_TOP);
  title_->SetLineHeight(kLineHeight);
  title_->SetMultiLine(true);
  title_->SetMaxLines(2);
  title_->SetEnabledColorId(cros_tokens::kTextColorPrimary);
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  GetViewAccessibility().SetName(title_->GetText(),
                                 ax::mojom::NameFrom::kAttribute);

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kItemBorderThickness, kItemBorderThickness,
                        kItemBorderThickness, kItemBorderThickness)));
}

PowerButtonMenuItemView::~PowerButtonMenuItemView() = default;

void PowerButtonMenuItemView::Layout(PassKey) {
  const gfx::Rect rect(GetContentsBounds());

  gfx::Rect icon_rect(rect);
  icon_rect.ClampToCenteredSize(gfx::Size(kIconSize, kIconSize));
  icon_rect.set_y(kIconTopPadding);
  icon_view_->SetBoundsRect(icon_rect);

  const int kTitleTopPadding =
      kTitleTopPaddingIncludesOneLineHeight - title_->font_list().GetHeight();
  title_->SetBoundsRect(gfx::Rect(0, kTitleTopPadding, kMenuItemWidth,
                                  kMenuItemHeight - kTitleTopPadding));
}

gfx::Size PowerButtonMenuItemView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kMenuItemWidth + 2 * kItemBorderThickness,
                   kMenuItemHeight + 2 * kItemBorderThickness);
}

void PowerButtonMenuItemView::OnFocus() {
  parent()->SetFocusBehavior(FocusBehavior::NEVER);
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  SchedulePaint();
}

void PowerButtonMenuItemView::OnBlur() {
  SchedulePaint();
}

void PowerButtonMenuItemView::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  // Set the background color to SK_ColorTRANSPARENT here since the parent view
  // PowerButtonMenuView has already set the background color.
  flags.setColor(SK_ColorTRANSPARENT);
  const gfx::Rect content_bounds = GetContentsBounds();
  canvas->DrawRoundRect(content_bounds, kFocusedItemRoundRectRadiusDp, flags);

  if (!HasFocus() || content_bounds.IsEmpty())
    return;

  // Border.
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(gfx::Insets(kItemBorderThickness));
  // Stroke.
  flags.setColor(GetColorProvider()->GetColor(ui::kColorAshFocusRing));
  flags.setStrokeWidth(kItemBorderThickness);
  flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  canvas->DrawRoundRect(bounds, kFocusedItemRoundRectRadiusDp, flags);
}

BEGIN_METADATA(PowerButtonMenuItemView)
END_METADATA

}  // namespace ash

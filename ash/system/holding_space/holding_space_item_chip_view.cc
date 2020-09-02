// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/system/user/rounded_image_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"

namespace ash {

HoldingSpaceItemChipView::HoldingSpaceItemChipView(const HoldingSpaceItem* item)
    : item_(item) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kHoldingSpaceChipPadding), kHoldingSpaceChipChildSpacing));

  image_ =
      AddChildView(std::make_unique<tray::RoundedImageView>(kTrayItemSize / 2));

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  layout->SetFlexForView(label_, 1);

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::HOLDING_SPACE_TITLE);
  style.SetupLabel(label_);

  AddPinButton();

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_ink_drop_visible_opacity(ShelfConfig::Get()->GetInkDropVisibleOpacity());
  set_notify_enter_exit_on_child(true);

  Update();
}

HoldingSpaceItemChipView::~HoldingSpaceItemChipView() = default;

const char* HoldingSpaceItemChipView::GetClassName() const {
  return "HoldingSpaceItemChipView";
}

SkColor HoldingSpaceItemChipView::GetInkDropBaseColor() const {
  return ShelfConfig::Get()->GetInkDropBaseColor();
}

void HoldingSpaceItemChipView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
      pin_->SetVisible(IsMouseHovered());
      break;
    case ui::ET_MOUSE_EXITED:
      pin_->SetVisible(IsMouseHovered());
      break;
    default:
      break;
  }
  views::InkDropHostView::OnMouseEvent(event);
}

void HoldingSpaceItemChipView::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  const AshColorProvider* color_provider = AshColorProvider::Get();
  SkColor color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
      AshColorProvider::AshColorMode::kDark);
  flags.setColor(color);

  canvas->DrawRoundRect(GetContentsBounds(), kHoldingSpaceChipCornerRadius,
                        flags);

  views::InkDropHostView::OnPaint(canvas);
}

void HoldingSpaceItemChipView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (sender == pin_) {
    pin_->SetToggled(!pin_->toggled());
    // TODO(amehfooz): Toggle pin
  }
}

void HoldingSpaceItemChipView::AddPinButton() {
  pin_ = AddChildView(std::make_unique<views::ToggleImageButton>(this));
  pin_->SetVisible(false);

  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSystemMenuIconColor,
      AshColorProvider::AshColorMode::kDark);

  const gfx::ImageSkia unpinned_icon =
      gfx::CreateVectorIcon(views::kUnpinIcon, icon_color);
  const gfx::ImageSkia pinned_icon =
      gfx::CreateVectorIcon(views::kPinIcon, icon_color);

  pin_->SetImage(views::Button::STATE_NORMAL, unpinned_icon);
  pin_->SetToggledImage(views::Button::STATE_NORMAL, &pinned_icon);
}

void HoldingSpaceItemChipView::Update() {
  image_->SetImage(item_->image().image_skia(), {kTrayItemSize, kTrayItemSize});
  label_->SetText(item_->text());
}

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/system/user/rounded_image_view.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"

namespace ash {

HoldingSpaceItemChipView::HoldingSpaceItemChipView(const HoldingSpaceItem* item)
    : HoldingSpaceItemView(item) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kHoldingSpaceChipPadding), kHoldingSpaceChipChildSpacing));

  SetPreferredSize(gfx::Size(kHoldingSpaceChipWidth, kHoldingSpaceChipHeight));

  image_ =
      AddChildView(std::make_unique<tray::RoundedImageView>(kTrayItemSize / 2));

  label_ = AddChildView(std::make_unique<views::Label>(item->text()));
  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  layout->SetFlexForView(label_, 1);

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::HOLDING_SPACE_TITLE);
  style.SetupLabel(label_);

  AddPinButton();

  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kHoldingSpaceChipCornerRadius));

  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  SetInkDropVisibleOpacity(
      ShelfConfig::Get()->GetInkDropRippleAttributes().inkdrop_opacity);
  SetNotifyEnterExitOnChild(true);

  // Ink drop layers should be clipped to match the corner radius of this view.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kHoldingSpaceChipCornerRadius);

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::Update, base::Unretained(this)));

  Update();
}

HoldingSpaceItemChipView::~HoldingSpaceItemChipView() = default;

void HoldingSpaceItemChipView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdatePin();
      break;
    default:
      break;
  }
  views::InkDropHostView::OnMouseEvent(event);
}

void HoldingSpaceItemChipView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  DCHECK_EQ(sender, pin_);
  bool is_item_pinned = HoldingSpaceController::Get()->model()->GetItem(
      HoldingSpaceItem::GetFileBackedItemId(HoldingSpaceItem::Type::kPinnedFile,
                                            item()->file_path()));
  pin_->SetToggled(!is_item_pinned);

  if (is_item_pinned)
    HoldingSpaceController::Get()->client()->UnpinItem(*item());
  else
    HoldingSpaceController::Get()->client()->PinItem(*item());

  UpdatePin();
}

void HoldingSpaceItemChipView::AddPinButton() {
  pin_ = AddChildView(std::make_unique<views::ToggleImageButton>(this));
  pin_->SetVisible(false);

  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSystemMenuIconColor);

  const gfx::ImageSkia unpinned_icon =
      gfx::CreateVectorIcon(views::kUnpinIcon, icon_color);
  const gfx::ImageSkia pinned_icon =
      gfx::CreateVectorIcon(views::kPinIcon, icon_color);

  pin_->SetImage(views::Button::STATE_NORMAL, unpinned_icon);
  pin_->SetToggledImage(views::Button::STATE_NORMAL, &pinned_icon);
}

void HoldingSpaceItemChipView::Update() {
  image_->SetImage(
      item()->image().image_skia(),
      gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize));
}

void HoldingSpaceItemChipView::UpdatePin() {
  if (!IsMouseHovered()) {
    pin_->SetVisible(false);
    return;
  }

  bool is_item_pinned = HoldingSpaceController::Get()->model()->GetItem(
      HoldingSpaceItem::GetFileBackedItemId(HoldingSpaceItem::Type::kPinnedFile,
                                            item()->file_path()));

  pin_->SetToggled(!is_item_pinned);
  pin_->SetVisible(true);
}

BEGIN_METADATA(HoldingSpaceItemChipView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash

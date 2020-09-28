// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "base/bind.h"
#include "ui/base/class_property.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {

// A UI class property used to identify if a view is an instance of
// `HoldingSpaceItemView`. Class name is not an adequate identifier as it may be
// overridden by subclasses.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsHoldingSpaceItemViewProperty, false)

}  // namespace

// HoldingSpaceItemView --------------------------------------------------------

HoldingSpaceItemView::HoldingSpaceItemView(
    HoldingSpaceItemViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : delegate_(delegate), item_(item) {
  SetProperty(kIsHoldingSpaceItemViewProperty, true);

  set_context_menu_controller(delegate_);
  set_drag_controller(delegate_);

  SetNotifyEnterExitOnChild(true);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  GetViewAccessibility().OverrideName(item->text());

  // Install the selection ring before installing the focus ring so that the
  // selection ring will paint beneath the focus ring.
  views::FocusRing* selection_ring = views::FocusRing::Install(this);
  selection_ring->SetColor(gfx::kPlaceholderColor);
  selection_ring->SetHasFocusPredicate(
      [this](views::View* selection_ring) { return this->selected(); });

  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing* focus_ring = views::FocusRing::Install(this);
  focus_ring->SetColor(ShelfConfig::Get()->shelf_focus_border_color());

  // The selection ring, focus ring, and ink drop layers should match the corner
  // radius of this view. Installation of a highlight path generator does this.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kHoldingSpaceCornerRadius);

  delegate_->OnHoldingSpaceItemViewCreated(this);
}

HoldingSpaceItemView::~HoldingSpaceItemView() {
  delegate_->OnHoldingSpaceItemViewDestroyed(this);
}

// static
HoldingSpaceItemView* HoldingSpaceItemView::Cast(views::View* view) {
  DCHECK(view->GetProperty(kIsHoldingSpaceItemViewProperty));
  return static_cast<HoldingSpaceItemView*>(view);
}

SkColor HoldingSpaceItemView::GetInkDropBaseColor() const {
  return AshColorProvider::Get()->GetRippleAttributes().base_color;
}

void HoldingSpaceItemView::OnGestureEvent(ui::GestureEvent* event) {
  delegate_->OnHoldingSpaceItemViewGestureEvent(this, *event);
}

bool HoldingSpaceItemView::OnKeyPressed(const ui::KeyEvent& event) {
  return delegate_->OnHoldingSpaceItemViewKeyPressed(this, event);
}

void HoldingSpaceItemView::OnMouseEvent(ui::MouseEvent* event) {
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

bool HoldingSpaceItemView::OnMousePressed(const ui::MouseEvent& event) {
  return delegate_->OnHoldingSpaceItemViewMousePressed(this, event);
}

void HoldingSpaceItemView::OnMouseReleased(const ui::MouseEvent& event) {
  delegate_->OnHoldingSpaceItemViewMouseReleased(this, event);
}

void HoldingSpaceItemView::SetSelected(bool selected) {
  if (selected_ == selected)
    return;

  selected_ = selected;
  InvalidateLayout();
}

void HoldingSpaceItemView::AddPin(views::View* parent) {
  DCHECK(!pin_);

  pin_ = parent->AddChildView(std::make_unique<views::ToggleImageButton>());
  pin_->SetVisible(false);

  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSystemMenuIconColor);

  const gfx::ImageSkia unpinned_icon =
      gfx::CreateVectorIcon(views::kUnpinIcon, icon_color);
  const gfx::ImageSkia pinned_icon =
      gfx::CreateVectorIcon(views::kPinIcon, icon_color);

  pin_->SetImage(views::Button::STATE_NORMAL, unpinned_icon);
  pin_->SetToggledImage(views::Button::STATE_NORMAL, &pinned_icon);
  pin_->set_callback(base::BindRepeating(&HoldingSpaceItemView::OnPinPressed,
                                         base::Unretained(this)));
}

void HoldingSpaceItemView::OnPinPressed() {
  const bool is_item_pinned = HoldingSpaceController::Get()->model()->GetItem(
      HoldingSpaceItem::GetFileBackedItemId(HoldingSpaceItem::Type::kPinnedFile,
                                            item()->file_path()));

  // Unpinning `item()` may result in the destruction of this view.
  auto weak_ptr = weak_factory_.GetWeakPtr();
  if (is_item_pinned)
    HoldingSpaceController::Get()->client()->UnpinItems({item()});
  else
    HoldingSpaceController::Get()->client()->PinItems({item()});

  if (weak_ptr)
    UpdatePin();
}

void HoldingSpaceItemView::UpdatePin() {
  if (!IsMouseHovered()) {
    pin_->SetVisible(false);
    return;
  }

  const bool is_item_pinned = HoldingSpaceController::Get()->model()->GetItem(
      HoldingSpaceItem::GetFileBackedItemId(HoldingSpaceItem::Type::kPinnedFile,
                                            item()->file_path()));

  pin_->SetToggled(!is_item_pinned);
  pin_->SetVisible(true);
}

BEGIN_METADATA(HoldingSpaceItemView, views::InkDropHostView)
END_METADATA

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ui/base/class_property.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

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

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  GetViewAccessibility().OverrideName(item->text());

  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing* focus_ring = views::FocusRing::Install(this);
  focus_ring->SetColor(ShelfConfig::Get()->shelf_focus_border_color());

  // Focus ring and ink drop layers should match the corner radius of this view.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kHoldingSpaceCornerRadius);
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
  return ShelfConfig::Get()->GetInkDropRippleAttributes().base_color;
}

void HoldingSpaceItemView::OnGestureEvent(ui::GestureEvent* event) {
  delegate_->OnHoldingSpaceItemViewGestureEvent(this, *event);
}

bool HoldingSpaceItemView::OnKeyPressed(const ui::KeyEvent& event) {
  return delegate_->OnHoldingSpaceItemViewKeyPressed(this, event);
}

bool HoldingSpaceItemView::OnMousePressed(const ui::MouseEvent& event) {
  return delegate_->OnHoldingSpaceItemViewMousePressed(this, event);
}

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_context_menu.h"
#include "base/bind_helpers.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Attempts to open the specified holding space `item`.
void OpenItem(const HoldingSpaceItem& item) {
  HoldingSpaceController::Get()->client()->OpenItem(item, base::DoNothing());
}

}  // namespace

// HoldingSpaceItemView --------------------------------------------------------

HoldingSpaceItemView::HoldingSpaceItemView(const HoldingSpaceItem* item)
    : item_(item),
      context_menu_(std::make_unique<HoldingSpaceItemContextMenu>(item_)) {
  set_context_menu_controller(context_menu_.get());

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

HoldingSpaceItemView::~HoldingSpaceItemView() = default;

int HoldingSpaceItemView::GetDragOperations(const gfx::Point& point) {
  return ui::DragDropTypes::DRAG_COPY;
}

SkColor HoldingSpaceItemView::GetInkDropBaseColor() const {
  return ShelfConfig::Get()->GetInkDropRippleAttributes().base_color;
}

void HoldingSpaceItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP)
    OpenItem(*item());
}

bool HoldingSpaceItemView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    OpenItem(*item());
    return true;
  }
  return false;
}

bool HoldingSpaceItemView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.flags() & ui::EF_IS_DOUBLE_CLICK) {
    OpenItem(*item());
    return true;
  }
  return false;
}

void HoldingSpaceItemView::WriteDragData(const gfx::Point& point,
                                         ui::OSExchangeData* data) {
  data->SetFilename(item_->file_path());
}

}  // namespace ash

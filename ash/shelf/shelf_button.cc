// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/animation/ink_drop_impl.h"

namespace ash {

ShelfButton::ShelfButton(Shelf* shelf,
                         ShelfButtonDelegate* shelf_button_delegate)
    : Button(nullptr),
      shelf_(shelf),
      shelf_button_delegate_(shelf_button_delegate) {
  DCHECK(shelf_button_delegate_);
  set_hide_ink_drop_when_showing_context_menu(false);
  set_ink_drop_base_color(ShelfConfig::Get()->shelf_ink_drop_base_color());
  set_ink_drop_visible_opacity(
      ShelfConfig::Get()->shelf_ink_drop_visible_opacity());
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      ShelfConfig::Get()->shelf_focus_border_color(), kFocusBorderThickness,
      gfx::InsetsF()));
}

ShelfButton::~ShelfButton() = default;

////////////////////////////////////////////////////////////////////////////////
// views::View

const char* ShelfButton::GetClassName() const {
  return "ash/ShelfButton";
}

void ShelfButton::AboutToRequestFocusFromTabTraversal(bool reverse) {
  shelf_button_delegate_->OnShelfButtonAboutToRequestFocusFromTabTraversal(
      this, reverse);
}

// Do not remove this function to avoid unnecessary ChromeVox announcement
// triggered by Button::GetAccessibleNodeData. (See https://crbug.com/932200)
void ShelfButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
}

////////////////////////////////////////////////////////////////////////////////
// views::Button

void ShelfButton::NotifyClick(const ui::Event& event) {
  // Pressing a shelf button in the auto-hide shelf should not do anything.
  // The event can still be received by the auto-hide shelf since we reserved
  // a portion of the auto-hide shelf within the screen bounds.
  if (!shelf_->IsVisible())
    return;

  Button::NotifyClick(event);
  if (shelf_button_delegate_)
    shelf_button_delegate_->ButtonPressed(/*sender=*/this, event, GetInkDrop());
}

std::unique_ptr<views::InkDrop> ShelfButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

}  // namespace ash

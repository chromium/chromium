// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button.h"

#include "ash/constants/ash_constants.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/style/style_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

ShelfButton::ShelfButton(Shelf* shelf,
                         ShelfButtonDelegate* shelf_button_delegate)
    : Button(Button::PressedCallback()),
      shelf_(shelf),
      shelf_button_delegate_(shelf_button_delegate) {
  DCHECK(shelf_button_delegate_);
  SetHideInkDropWhenShowingContextMenu(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // Inset focus ring path to avoid clipping the edges of the ring.
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  views::FocusRing::Get(this)->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          gfx::Insets(-views::FocusRing::kDefaultHaloInset)));
  SetFocusPainter(nullptr);
}

ShelfButton::~ShelfButton() = default;

////////////////////////////////////////////////////////////////////////////////
// views::View

void ShelfButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  if (chromeos::features::IsJellyEnabled()) {
    auto* ink_drop = views::InkDrop::Get(this);
    ink_drop->SetBaseColorId(cros_tokens::kCrosSysRippleNeutralOnSubtle);
    ink_drop->SetVisibleOpacity(1.0f);
  } else {
    StyleUtil::ConfigureInkDropAttributes(
        this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
  }
}

const char* ShelfButton::GetClassName() const {
  return "ash/ShelfButton";
}

gfx::Rect ShelfButton::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = Button::GetAnchorBoundsInScreen();
  // Padding used to position bubbles offset from the shelf. Note that this
  // includes Shelf tooltip.
  constexpr int kAnchorOffset = 6;
  bounds.Inset(gfx::Insets(-kAnchorOffset));
  return bounds;
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
    shelf_button_delegate_->ButtonPressed(
        /*sender=*/this, event, views::InkDrop::Get(this)->GetInkDrop());
}

}  // namespace ash

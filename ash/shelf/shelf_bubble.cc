// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_bubble.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/views/bubble/bubble_frame_view.h"

namespace {

views::BubbleBorder::Arrow GetArrow(ash::ShelfAlignment alignment) {
  switch (alignment) {
    case ash::ShelfAlignment::kBottom:
    case ash::ShelfAlignment::kBottomLocked:
      return views::BubbleBorder::BOTTOM_CENTER;
    case ash::ShelfAlignment::kLeft:
      return views::BubbleBorder::LEFT_CENTER;
    case ash::ShelfAlignment::kRight:
      return views::BubbleBorder::RIGHT_CENTER;
  }
  return views::BubbleBorder::Arrow::NONE;
}

}  // namespace

namespace ash {

ShelfBubble::ShelfBubble(views::View* anchor, ShelfAlignment alignment)
    : views::BubbleDialogDelegateView(anchor, GetArrow(alignment)),
      background_animator_(
          /* Don't pass the Shelf so the translucent color is always used. */
          nullptr,
          Shell::Get()->wallpaper_controller()) {
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  background_animator_.Init(ShelfBackgroundType::kDefaultBg);
  background_animator_.AddObserver(this);

  // Place the bubble in the same display as the anchor.
  set_parent_window(
      anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
          kShellWindowId_SettingBubbleContainer));
  // We override the role because the base class sets it to alert dialog,
  // which results in each tooltip title being announced twice on screen
  // readers each time it is shown.
  SetAccessibleRole(ax::mojom::Role::kDialog);
}

ShelfBubble::~ShelfBubble() {
  background_animator_.RemoveObserver(this);
}

void ShelfBubble::CreateBubble() {
  // Actually create the bubble.
  views::BubbleDialogDelegateView::CreateBubble(this);

  // Settings that should only be changed just after bubble creation.
  GetBubbleFrameView()->SetCornerRadius(border_radius_);
  GetBubbleFrameView()->SetBackgroundColor(color());
}

void ShelfBubble::UpdateShelfBackground(SkColor color) {
  set_color(color);
}

}  // namespace ash

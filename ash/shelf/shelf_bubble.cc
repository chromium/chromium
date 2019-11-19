// Copyright 2018 The Chromium Authors. All rights reserved.
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
    case ash::SHELF_ALIGNMENT_BOTTOM:
    case ash::SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return views::BubbleBorder::BOTTOM_CENTER;
    case ash::SHELF_ALIGNMENT_LEFT:
      return views::BubbleBorder::LEFT_CENTER;
    case ash::SHELF_ALIGNMENT_RIGHT:
      return views::BubbleBorder::RIGHT_CENTER;
  }
  return views::BubbleBorder::Arrow::NONE;
}

}  // namespace

namespace ash {

ShelfBubble::ShelfBubble(views::View* anchor,
                         ShelfAlignment alignment,
                         SkColor background_color)
    : views::BubbleDialogDelegateView(anchor, GetArrow(alignment)),
      background_animator_(
          /* Don't pass the Shelf so the translucent color is always used. */
          nullptr,
          Shell::Get()->wallpaper_controller()) {
  background_animator_.Init(SHELF_BACKGROUND_DEFAULT);
  background_animator_.AddObserver(this);

  // Place the bubble in the same display as the anchor.
  set_parent_window(
      anchor_widget()->GetNativeWindow()->GetRootWindow()->GetChildById(
          kShellWindowId_SettingBubbleContainer));
}

ShelfBubble::~ShelfBubble() {
  background_animator_.RemoveObserver(this);
}

ax::mojom::Role ShelfBubble::GetAccessibleWindowRole() {
  // We override the role because the base class sets it to alert dialog,
  // which results in each tooltip title being announced twice on screen
  // readers each time it is shown.
  return ax::mojom::Role::kDialog;
}

void ShelfBubble::CreateBubble() {
  // Actually create the bubble.
  views::BubbleDialogDelegateView::CreateBubble(this);

  // Settings that should only be changed just after bubble creation.
  GetBubbleFrameView()->SetCornerRadius(border_radius_);
  GetBubbleFrameView()->SetBackgroundColor(color());
}

int ShelfBubble::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void ShelfBubble::UpdateShelfBackground(SkColor color) {
  set_color(color);
}

}  // namespace ash

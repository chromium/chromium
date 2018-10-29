// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble.h"

#include "ash/focus_cycler.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverflowBubble::OverflowBubble(Shelf* shelf)
    : shelf_(shelf), bubble_(nullptr), overflow_button_(nullptr) {
  DCHECK(shelf_);
  Shell::Get()->AddPreTargetHandler(this);
}

OverflowBubble::~OverflowBubble() {
  Hide();
  Shell::Get()->RemovePreTargetHandler(this);
}

void OverflowBubble::Show(OverflowButton* overflow_button,
                          ShelfView* shelf_view) {
  DCHECK(overflow_button);
  DCHECK(shelf_view);

  Hide();

  bubble_ = new OverflowBubbleView(shelf_);
  bubble_->InitOverflowBubble(overflow_button, shelf_view);
  overflow_button_ = overflow_button;

  TrayBackgroundView::InitializeBubbleAnimations(bubble_->GetWidget());
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();
  Shell::Get()->focus_cycler()->AddWidget(bubble_->GetWidget());
}

void OverflowBubble::Hide() {
  if (!IsShowing())
    return;

  Shell::Get()->focus_cycler()->RemoveWidget(bubble_->GetWidget());
  bubble_->GetWidget()->RemoveObserver(this);
  bubble_->GetWidget()->Close();
  bubble_ = nullptr;
  overflow_button_ = nullptr;
}

void OverflowBubble::ProcessPressedEvent(ui::LocatedEvent* event) {
  if (!IsShowing() || bubble_->shelf_view()->IsShowingMenu())
    return;

  const gfx::Point screen_location = event->target()->GetScreenLocation(*event);
  if (bubble_->GetBoundsInScreen().Contains(screen_location) ||
      overflow_button_->GetBoundsInScreen().Contains(screen_location)) {
    return;
  }

  // Do not hide the shelf if one of the buttons on the main shelf was pressed,
  // since the user might want to drag an item onto the overflow bubble.
  // The button itself will close the overflow bubble on the release event.
  if (bubble_->shelf_view()
          ->main_shelf()
          ->GetVisibleItemsBoundsInScreen()
          .Contains(screen_location)) {
    return;
  }

  Hide();
}

void OverflowBubble::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(event);
}

void OverflowBubble::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(event);
}

void OverflowBubble::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(widget == bubble_->GetWidget());
  // Update the overflow button in the parent ShelfView.
  overflow_button_->SchedulePaint();
  bubble_ = nullptr;
  overflow_button_ = nullptr;
}

}  // namespace ash

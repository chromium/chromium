// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_tooltip_bubble.h"
#include "ash/shelf/shelf_tooltip_preview_bubble.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/chromeos_switches.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

const int kTooltipAppearanceDelay = 1000;  // msec

}  // namespace

ShelfTooltipManager::ShelfTooltipManager(ShelfView* shelf_view)
    : timer_delay_(kTooltipAppearanceDelay),
      shelf_view_(shelf_view),
      weak_factory_(this) {
  shelf_view_->shelf()->AddObserver(this);
  Shell::Get()->AddPreTargetHandler(this);
}

ShelfTooltipManager::~ShelfTooltipManager() {
  Shell::Get()->RemovePreTargetHandler(this);
  shelf_view_->shelf()->RemoveObserver(this);
  if (shelf_view_->GetWidget() && shelf_view_->GetWidget()->GetNativeWindow())
    shelf_view_->GetWidget()->GetNativeWindow()->RemovePreTargetHandler(this);
}

void ShelfTooltipManager::Close(bool animate) {
  // Cancel any timer set to show a tooltip after a delay.
  timer_.Stop();
  if (!bubble_)
    return;
  if (!animate) {
    // Cancel the typical hiding animation to hide the bubble immediately.
    ::wm::SetWindowVisibilityAnimationTransition(
        bubble_->GetWidget()->GetNativeWindow(), ::wm::ANIMATE_NONE);
  }
  bubble_->GetWidget()->Close();
  bubble_ = nullptr;
}

bool ShelfTooltipManager::IsVisible() const {
  return bubble_ && bubble_->GetWidget()->IsVisible();
}

views::View* ShelfTooltipManager::GetCurrentAnchorView() const {
  return bubble_ ? bubble_->GetAnchorView() : nullptr;
}

void ShelfTooltipManager::ShowTooltip(views::View* view) {
  // Hide the old bubble immediately, skipping the typical closing animation.
  Close(false /*animate*/);

  if (!ShouldShowTooltipForView(view))
    return;

  views::BubbleBorder::Arrow arrow = views::BubbleBorder::Arrow::NONE;
  switch (shelf_view_->shelf()->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      arrow = views::BubbleBorder::BOTTOM_CENTER;
      break;
    case SHELF_ALIGNMENT_LEFT:
      arrow = views::BubbleBorder::LEFT_CENTER;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      arrow = views::BubbleBorder::RIGHT_CENTER;
      break;
  }

  const std::vector<aura::Window*> open_windows =
      shelf_view_->GetOpenWindowsForShelfView(view);

  const base::string16 text = shelf_view_->GetTitleForView(view);
  if (chromeos::switches::ShouldShowShelfHoverPreviews() &&
      open_windows.size() > 0) {
    bubble_ = new ShelfTooltipPreviewBubble(view, arrow, open_windows, this);
  } else {
    bubble_ = new ShelfTooltipBubble(view, arrow, text);
  }

  aura::Window* window = bubble_->GetWidget()->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_HIDE);
  bubble_->GetWidget()->Show();
}

void ShelfTooltipManager::ShowTooltipWithDelay(views::View* view) {
  if (ShouldShowTooltipForView(view)) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timer_delay_),
                 base::Bind(&ShelfTooltipManager::ShowTooltip,
                            weak_factory_.GetWeakPtr(), view));
  }
}

void ShelfTooltipManager::OnMouseEvent(ui::MouseEvent* event) {
  if (bubble_ && event->type() == ui::ET_MOUSE_PRESSED) {
    ProcessPressedEvent(*event);
    return;
  }

  if (bubble_ && event->type() == ui::ET_MOUSE_EXITED &&
      bubble_->ShouldCloseOnMouseExit()) {
    Close();
    return;
  }

  // The code below handles mouse move events within the shelf window.
  if (event->type() != ui::ET_MOUSE_MOVED ||
      event->target() != shelf_view_->GetWidget()->GetNativeWindow()) {
    return;
  }

  gfx::Point point = event->location();
  views::View::ConvertPointFromWidget(shelf_view_, &point);
  views::View* view = shelf_view_->GetTooltipHandlerForPoint(point);
  const bool should_show = ShouldShowTooltipForView(view);

  timer_.Stop();
  if (IsVisible() && should_show && bubble_->GetAnchorView() != view)
    ShowTooltip(view);
  else if (!IsVisible() && should_show)
    ShowTooltipWithDelay(view);
  else if (IsVisible() && shelf_view_->ShouldHideTooltip(point))
    Close();
}

void ShelfTooltipManager::OnTouchEvent(ui::TouchEvent* event) {
  if (bubble_ && event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(*event);
}

void ShelfTooltipManager::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  if (new_state == SHELF_HIDDEN)
    Close();
}

void ShelfTooltipManager::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  if (new_state == SHELF_AUTO_HIDE_HIDDEN)
    Close();
}

bool ShelfTooltipManager::ShouldShowTooltipForView(views::View* view) {
  Shelf* shelf = shelf_view_ ? shelf_view_->shelf() : nullptr;
  return shelf && shelf_view_->visible() &&
         shelf_view_->ShouldShowTooltipForView(view) &&
         (shelf->GetVisibilityState() == SHELF_VISIBLE ||
          (shelf->GetVisibilityState() == SHELF_AUTO_HIDE &&
           shelf->GetAutoHideState() == SHELF_AUTO_HIDE_SHOWN));
}

void ShelfTooltipManager::ProcessPressedEvent(const ui::LocatedEvent& event) {
  // Always close the tooltip on press events outside the tooltip.
  if (bubble_->ShouldCloseOnPressDown() ||
      event.target() != bubble_->GetWidget()->GetNativeWindow()) {
    Close();
  }
}

}  // namespace ash

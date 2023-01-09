// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_tooltip_manager.h"

#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_tooltip_bubble.h"
#include "ash/shelf/shelf_tooltip_delegate.h"
#include "ash/shelf/shelf_tooltip_preview_bubble.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

const int kTooltipAppearanceDelay = 250;  // msec

}  // namespace

ShelfTooltipManager::ShelfTooltipManager(Shelf* shelf)
    : timer_delay_(kTooltipAppearanceDelay), shelf_(shelf) {
  shelf_->AddObserver(this);
  Shell::Get()->AddPreTargetHandler(this);
}

ShelfTooltipManager::~ShelfTooltipManager() {
  Shell::Get()->RemovePreTargetHandler(this);
  shelf_->RemoveObserver(this);
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

  const std::vector<aura::Window*> open_windows =
      shelf_tooltip_delegate_->GetOpenWindowsForView(view);

  const ShelfAlignment alignment = shelf_->alignment();
  if (switches::ShouldShowShelfHoverPreviews() && open_windows.size() > 0) {
    bubble_ =
        new ShelfTooltipPreviewBubble(view, open_windows, this, alignment);
  } else {
    bubble_ = new ShelfTooltipBubble(
        view, alignment, shelf_tooltip_delegate_->GetTitleForView(view));
  }

  aura::Window* window = bubble_->GetWidget()->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_HIDE);
  // Do not trigger a highlight when hovering over shelf items.
  bubble_->set_highlight_button_when_shown(false);
  bubble_->GetWidget()->Show();
}

void ShelfTooltipManager::ShowTooltipWithDelay(views::View* view) {
  if (ShouldShowTooltipForView(view)) {
    timer_.Start(FROM_HERE, base::Milliseconds(timer_delay_),
                 base::BindOnce(&ShelfTooltipManager::ShowTooltip,
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

  // Happens in tests where mouse events are picked up before
  // |shelf_tooltip_delegate_| is set.
  if (!shelf_tooltip_delegate_)
    return;

  views::View* delegate_view = shelf_tooltip_delegate_->GetViewForEvent(*event);

  // The code below handles mouse move events within the shelf window.
  if (event->type() != ui::ET_MOUSE_MOVED || !delegate_view) {
    // Don't show delayed tooltips if the mouse is being active elsewhere.
    timer_.Stop();
    return;
  }

  gfx::Point point = event->location();
  views::View::ConvertPointFromWidget(delegate_view, &point);
  views::View* view = delegate_view->GetTooltipHandlerForPoint(point);
  const bool should_show = ShouldShowTooltipForView(view);

  timer_.Stop();
  if (IsVisible() && should_show && bubble_->GetAnchorView() != view)
    ShowTooltip(view);
  else if (!IsVisible() && should_show)
    ShowTooltipWithDelay(view);
  else if (IsVisible() && shelf_tooltip_delegate_->ShouldHideTooltip(point))
    Close();
}

void ShelfTooltipManager::OnTouchEvent(ui::TouchEvent* event) {
  if (bubble_ && event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(*event);
}

void ShelfTooltipManager::OnScrollEvent(ui::ScrollEvent* event) {
  // Close any currently shown bubble.
  Close();
}

void ShelfTooltipManager::OnKeyEvent(ui::KeyEvent* event) {
  // Close any currently shown bubble.
  Close();
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
  const bool shelf_visibility =
      shelf_ && (shelf_->GetVisibilityState() == SHELF_VISIBLE ||
                 (shelf_->GetVisibilityState() == SHELF_AUTO_HIDE &&
                  shelf_->GetAutoHideState() == SHELF_AUTO_HIDE_SHOWN));

  if (!shelf_visibility)
    return false;
  return shelf_tooltip_delegate_->ShouldShowTooltipForView(view);
}

void ShelfTooltipManager::ProcessPressedEvent(const ui::LocatedEvent& event) {
  // Always close the tooltip on press events outside the tooltip.
  if (bubble_->ShouldCloseOnPressDown() ||
      event.target() != bubble_->GetWidget()->GetNativeWindow()) {
    Close();
  }
}

}  // namespace ash

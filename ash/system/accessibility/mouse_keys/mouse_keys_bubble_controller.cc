// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_bubble_controller.h"

#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/mouse_keys/mouse_keys_bubble_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"

namespace ash {

MouseKeysBubbleController::MouseKeysBubbleController() = default;

MouseKeysBubbleController::~MouseKeysBubbleController() {
  if (widget_ && !widget_->IsClosed()) {
    widget_->CloseNow();
  }
}

void MouseKeysBubbleController::UpdateBubble(
    bool visible,
    MouseKeysBubbleIconType icon,
    const std::optional<std::u16string>& text) {
  EnsureInitialize();

  gfx::Point cursor_position =
      Shell::Get()->mouse_keys_controller()->GetLastMousePositionDips();
  cursor_position.Offset(16, 16);

  if (mouse_keys_bubble_view_) {
    mouse_keys_bubble_view_->SetAnchorRect(
        gfx::Rect(cursor_position, gfx::Size()));
  }

  Update(icon, text);
  widget_->SetVisible(visible);
  Update(icon, text);
}

void MouseKeysBubbleController::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view != mouse_keys_bubble_view_) {
    return;
  }

  mouse_keys_bubble_view_->views::View::RemoveObserver(this);
  mouse_keys_bubble_view_ = nullptr;
  // TODO(crbug.com/380053616) implement logic to show or hide widget.
  widget_ = nullptr;
}

void MouseKeysBubbleController::EnsureInitialize() {
  if (widget_) {
    return;
  }

  mouse_keys_bubble_view_ = new MouseKeysBubbleView();
  mouse_keys_bubble_view_->views::View::AddObserver(this);

  widget_ =
      views::BubbleDialogDelegateView::CreateBubble(mouse_keys_bubble_view_);
  widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kMouseKeysBubble);
}

void MouseKeysBubbleController::Update(
    MouseKeysBubbleIconType icon,
    const std::optional<std::u16string>& text) {
  DCHECK(mouse_keys_bubble_view_);
  DCHECK(widget_);

  // Update `mouse_keys_bubble_view_`.
  mouse_keys_bubble_view_->Update(icon, text);

  // Update the bounds to fit entirely within the screen.
  gfx::Rect new_bounds = widget_->GetWindowBoundsInScreen();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetDisplayMatching(new_bounds).bounds();
  new_bounds.AdjustToFit(display_bounds);

  // Update the preferred bounds based on other system windows.
  gfx::Rect resting_bounds = CollisionDetectionUtils::AvoidObstacles(
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget_->GetNativeWindow()),
      new_bounds, CollisionDetectionUtils::RelativePriority::kMouseKeysBubble);
  widget_->SetBounds(resting_bounds);
}

}  // namespace ash

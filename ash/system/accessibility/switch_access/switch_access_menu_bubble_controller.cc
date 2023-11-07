// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access/switch_access_menu_bubble_controller.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access/switch_access_back_button_bubble_controller.h"
#include "ash/system/accessibility/switch_access/switch_access_menu_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/views/border.h"

namespace ash {

SwitchAccessMenuBubbleController::SwitchAccessMenuBubbleController()
    : back_button_controller_(
          std::make_unique<SwitchAccessBackButtonBubbleController>()) {}

SwitchAccessMenuBubbleController::~SwitchAccessMenuBubbleController() {
  if (widget_ && !widget_->IsClosed())
    widget_->CloseNow();
}

void SwitchAccessMenuBubbleController::ShowBackButton(const gfx::Rect& anchor) {
  back_button_controller_->ShowBackButton(anchor, /*show_focus_ring=*/true,
                                          menu_open_);
}

void SwitchAccessMenuBubbleController::ShowMenu(
    const gfx::Rect& anchor,
    const std::vector<std::string>& actions_to_show) {
  menu_open_ = true;
  if (!widget_) {
    TrayBubbleView::InitParams init_params;
    init_params.delegate = GetWeakPtr();
    // Anchor within the overlay container.
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
    init_params.is_anchored_to_status_area = false;
    init_params.insets =
        gfx::Insets::VH(kBubbleMenuPadding, kBubbleMenuPadding);
    init_params.translucent = true;
    init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;

    bubble_view_ = new TrayBubbleView(init_params);
    bubble_view_->SetArrow(views::BubbleBorder::Arrow::TOP_LEFT);

    menu_view_ = new SwitchAccessMenuView();
    menu_view_->SetBorder(views::CreateEmptyBorder(kBubbleMenuPadding));
    bubble_view_->AddChildView(menu_view_.get());

    widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    TrayBackgroundView::InitializeBubbleAnimations(widget_);
    bubble_view_->InitializeAndShowBubble();

    CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
        widget_->GetNativeWindow(),
        CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu);
  }

  DCHECK(bubble_view_);

  menu_view_->SetActions(actions_to_show);
  bubble_view_->SetPreferredWidth(menu_view_->GetBubbleWidthDip());
  bubble_view_->ChangeAnchorRect(anchor);

  gfx::Rect new_bounds = widget_->GetWindowBoundsInScreen();

  // Adjust the bounds to fit entirely within the screen.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetDisplayMatching(new_bounds).bounds();
  new_bounds.AdjustToFit(display_bounds);

  // Update the preferred bounds based on other system windows.
  gfx::Rect resting_bounds = CollisionDetectionUtils::AvoidObstacles(
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget_->GetNativeWindow()),
      new_bounds, CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu);

  widget_->SetBounds(resting_bounds);
  widget_->Show();
  bubble_view_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                         true);

  // The resting bounds includes padding on each side of the menu.
  // Remove that before passing to the back button controller so the back button
  // appears in the correct position.
  resting_bounds.Inset(kBubbleMenuPadding);
  back_button_controller_->ShowBackButton(resting_bounds,
                                          /*show_focus_ring=*/false,
                                          /*for_menu=*/true);
}

void SwitchAccessMenuBubbleController::HideBackButton() {
  if (widget_ && widget_->IsVisible())
    back_button_controller_->HideFocusRing();
  else
    back_button_controller_->Hide();
}

void SwitchAccessMenuBubbleController::HideMenuBubble() {
  menu_open_ = false;
  back_button_controller_->Hide();
  if (widget_)
    widget_->Hide();
  if (bubble_view_)
    bubble_view_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                           true);
}

void SwitchAccessMenuBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  menu_view_ = nullptr;
  widget_ = nullptr;
}

void SwitchAccessMenuBubbleController::HideBubble(
    const TrayBubbleView* bubble_view) {
  // This function is currently not unused for bubbles of type
  // `kAccessibilityBubble`, so can leave this empty.
}

}  // namespace ash

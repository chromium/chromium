// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_back_button_bubble_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access_back_button_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

SwitchAccessBackButtonBubbleController::
    SwitchAccessBackButtonBubbleController() {}

SwitchAccessBackButtonBubbleController::
    ~SwitchAccessBackButtonBubbleController() {
  if (widget_ && !widget_->IsClosed())
    widget_->CloseNow();
}

void SwitchAccessBackButtonBubbleController::ShowBackButton(
    const gfx::Rect& anchor,
    bool show_focus_ring,
    bool for_menu) {
  if (!widget_) {
    back_button_view_ = new SwitchAccessBackButtonView(for_menu);

    TrayBubbleView::InitParams init_params;
    init_params.delegate = this;
    // Anchor within the overlay container.
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
    init_params.is_anchored_to_status_area = false;
    init_params.has_shadow = false;
    init_params.preferred_width = back_button_view_->size().width();
    init_params.translucent = true;

    bubble_view_ = new TrayBubbleView(init_params);
    bubble_view_->SetArrow(views::BubbleBorder::BOTTOM_RIGHT);
    bubble_view_->AddChildView(back_button_view_);
    bubble_view_->SetPaintToLayer();
    bubble_view_->layer()->SetFillsBoundsOpaquely(false);

    widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    TrayBackgroundView::InitializeBubbleAnimations(widget_);
    bubble_view_->InitializeAndShowBubble();
  } else {
    back_button_view_->SetForMenu(for_menu);
  }

  DCHECK(bubble_view_);
  back_button_view_->SetFocusRing(show_focus_ring);
  bubble_view_->ChangeAnchorRect(AdjustAnchorRect(anchor));
  widget_->Show();
  bubble_view_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                         true);
}

void SwitchAccessBackButtonBubbleController::HideFocusRing() {
  back_button_view_->SetFocusRing(false);
}

void SwitchAccessBackButtonBubbleController::Hide() {
  if (widget_)
    widget_->Hide();
  if (bubble_view_)
    bubble_view_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                           true);
}

void SwitchAccessBackButtonBubbleController::BubbleViewDestroyed() {
  back_button_view_ = nullptr;
  bubble_view_ = nullptr;
  widget_ = nullptr;
}

// The back button should display above and to the right of the anchor rect
// provided. Because the TrayBubbleView defaults to showing the right edges
// lining up (rather than appearing off to the side) we'll add the width of the
// button to the anchor rect's width.
gfx::Rect SwitchAccessBackButtonBubbleController::AdjustAnchorRect(
    const gfx::Rect& anchor) {
  DCHECK(back_button_view_);
  gfx::Size button_size = back_button_view_->size();

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetDisplayMatching(anchor).bounds();
  // Ensure that the back button displays onscreen by leaving space for the
  // button at the appropriate edges (the button displays at the top right
  // corner).
  display_bounds.Inset(0, button_size.height(), button_size.width(), 0);

  gfx::Rect adjusted_anchor(anchor);
  if (adjusted_anchor.Intersects(display_bounds)) {
    adjusted_anchor.Intersect(display_bounds);
  } else {
    // When the supplied anchor is entirely within our padding around the edges,
    // construct an anchor at the appropriate position.
    int x = adjusted_anchor.right();
    int y = adjusted_anchor.y();
    if (x < display_bounds.x()) {
      x = display_bounds.x();
    } else if (x > display_bounds.right()) {
      x = display_bounds.right();
    }
    if (y < display_bounds.y()) {
      y = display_bounds.y();
    } else if (y > display_bounds.bottom()) {
      y = display_bounds.bottom();
    }
    adjusted_anchor = gfx::Rect(x, y, 0, 0);
  }

  // The bubble aligns its right edge with the rect's right edge, so add the
  // button width to have them adjacent only at the corner.
  adjusted_anchor.set_width(adjusted_anchor.width() + button_size.width());
  return adjusted_anchor;
}

}  // namespace ash

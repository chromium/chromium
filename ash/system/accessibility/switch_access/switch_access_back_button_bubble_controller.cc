// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access/switch_access_back_button_bubble_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access/switch_access_back_button_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor/layer.h"
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
  for_menu_ = for_menu;

  if (!widget_) {
    back_button_view_ = new SwitchAccessBackButtonView(for_menu);

    TrayBubbleView::InitParams init_params;
    init_params.delegate = GetWeakPtr();
    // Anchor within the overlay container.
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
    init_params.is_anchored_to_status_area = false;
    init_params.preferred_width = back_button_view_->size().width();
    init_params.translucent = true;
    init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;

    bubble_view_ = new TrayBubbleView(init_params);
    bubble_view_->SetArrow(views::BubbleBorder::BOTTOM_RIGHT);
    bubble_view_->AddChildView(back_button_view_.get());

    // Only call `SetPaintToLayer()` when necessary since a layer could have
    // been created for `ViewShadow` and re-creating here breaks the z-order set
    // by `ViewShadow`.
    if (!bubble_view_->layer())
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

void SwitchAccessBackButtonBubbleController::HideBubble(
    const TrayBubbleView* bubble_view) {
  // This function is currently not unused for bubbles of type
  // `kAccessibilityBubble`, so can leave this empty.
}

// The back button should display to the right of the anchor rect provided.
// Because the TrayBubbleView defaults to showing the right edges lining up
// (rather than the top edges lining up) we'll add the width of the button to
// the anchor rect's width, and the height of the button to the y-coordinate.
gfx::Rect SwitchAccessBackButtonBubbleController::AdjustAnchorRect(
    const gfx::Rect& anchor) {
  DCHECK(back_button_view_);
  gfx::Size button_size = back_button_view_->size();
  gfx::Rect adjusted_anchor(anchor);

  // The bubble aligns its right edge with the rect's right edge, so add the
  // button width to have them adjacent only at the corner.
  int width = adjusted_anchor.width() + button_size.width();
  if (!for_menu_)
    width += kFocusRingPaddingDp;
  adjusted_anchor.set_width(width);

  // To align the top edges, move the button down by its height.
  int offset_height = button_size.height();
  if (for_menu_)
    offset_height -= back_button_view_->GetFocusRingWidthPerSide();
  else
    offset_height -= kFocusRingPaddingDp;
  adjusted_anchor.Offset(0, offset_height);

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetDisplayMatching(anchor).bounds();
  // Ensure that the back button displays onscreen.
  display_bounds.Inset(gfx::Insets::TLBR(button_size.height(), 0, 0, 0));

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

  return adjusted_anchor;
}

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/select_to_speak_menu_bubble_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/floating_menu_utils.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"

namespace ash {

namespace {
const int kAnchorRectVerticalSpacing = 12;
const int kPreferredWidth = 324;
}  // namespace

SelectToSpeakMenuBubbleController::SelectToSpeakMenuBubbleController() =
    default;

SelectToSpeakMenuBubbleController::~SelectToSpeakMenuBubbleController() {
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
}

void SelectToSpeakMenuBubbleController::Show(const gfx::Rect& anchor,
                                             bool is_paused) {
  if (!bubble_widget_) {
    TrayBubbleView::InitParams init_params;
    init_params.delegate = this;
    init_params.parent_window =
        Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                            kShellWindowId_AccessibilityBubbleContainer);
    init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
    init_params.is_anchored_to_status_area = false;
    init_params.insets = gfx::Insets(kUnifiedMenuPadding, kUnifiedMenuPadding);
    init_params.corner_radius = kUnifiedTrayCornerRadius;
    init_params.has_shadow = false;
    init_params.translucent = true;
    init_params.preferred_width = kPreferredWidth;
    bubble_view_ = new TrayBubbleView(init_params);
    bubble_view_->SetArrow(views::BubbleBorder::TOP_LEFT);

    menu_view_ = new SelectToSpeakMenuView();
    menu_view_->SetBorder(
        views::CreateEmptyBorder(kUnifiedTopShortcutSpacing, 0, 0, 0));
    bubble_view_->AddChildView(menu_view_);
    menu_view_->SetPaintToLayer();
    menu_view_->layer()->SetFillsBoundsOpaquely(false);

    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
    bubble_view_->InitializeAndShowBubble();
  }

  // Update button states.
  menu_view_->SetPaused(is_paused);

  // Add vertical spacing to given anchor rect.
  bubble_view_->ChangeAnchorRect(gfx::Rect(
      anchor.x(), anchor.y() - kAnchorRectVerticalSpacing, anchor.width(),
      anchor.height() + kAnchorRectVerticalSpacing * 2));
  bubble_widget_->Show();
}

void SelectToSpeakMenuBubbleController::Hide() {
  if (!bubble_widget_)
    return;
  bubble_widget_->Hide();
}

void SelectToSpeakMenuBubbleController::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;
  menu_view_ = nullptr;
}

}  // namespace ash
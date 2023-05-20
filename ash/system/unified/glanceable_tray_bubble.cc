// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble.h"

#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/glanceable_tray_bubble_view.h"
#include "ash/system/unified/tasks_bubble_view.h"

namespace ash {

GlanceableTrayBubble::GlanceableTrayBubble(DateTray* tray) : tray_(tray) {
  TrayBubbleView::InitParams init_params;
  init_params.shelf_alignment = tray_->shelf()->alignment();
  // TODO(b:277268122): Update with glanceable spec.
  init_params.preferred_width = kRevampedTrayMenuWidth;
  init_params.delegate = tray->GetWeakPtr();
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = tray_->shelf()->GetSystemTrayAnchorRect();
  // TODO(b:277268122): Update with glanceable spec.
  init_params.insets = GetTrayBubbleInsets();
  init_params.close_on_deactivate = false;
  init_params.reroute_event_handler = true;
  init_params.translucent = true;

  bubble_view_ = new GlanceableTrayBubbleView(init_params, tray_->shelf());

  // bubble_widget_ takes ownership of the bubble_view_.
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  tray->tray_event_filter()->AddBubble(this);
  bubble_view_->UpdateBubble();
}

GlanceableTrayBubble::~GlanceableTrayBubble() {
  tray_->tray_event_filter()->RemoveBubble(this);

  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

void GlanceableTrayBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;

  // `tray_->CloseBubble()` will delete `this`.
  tray_->CloseBubble();
}

TrayBackgroundView* GlanceableTrayBubble::GetTray() const {
  return tray_;
}

TrayBubbleView* GlanceableTrayBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* GlanceableTrayBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

TasksBubbleView* GlanceableTrayBubble::GetTasksView() const {
  return bubble_view_->GetTasksView();
}

bool GlanceableTrayBubble::IsBubbleActive() const {
  return bubble_widget_->IsActive();
}

}  // namespace ash

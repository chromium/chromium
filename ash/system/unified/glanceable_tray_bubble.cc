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
  TrayBubbleView::InitParams init_params =
      CreateInitParamsForTrayBubble(tray, /*anchor_to_shelf_corner=*/true);
  // TODO(b:277268122): Update with glanceable spec.
  init_params.preferred_width = kRevampedTrayMenuWidth;
  init_params.transparent = true;
  init_params.has_shadow = false;
  init_params.translucent = false;

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

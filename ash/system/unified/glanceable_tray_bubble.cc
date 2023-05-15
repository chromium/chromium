// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble.h"

#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/tray/tray_utils.h"

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

  bubble_view_ = new TrayBubbleView(init_params);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  tray->tray_event_filter()->AddBubble(this);
  UpdateBubble();
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

void GlanceableTrayBubble::UpdateBubble() {
  // TODO(b:277268122): set real contents for glanceables view.
  if (!title_label_) {
    title_label_ = bubble_view_->AddChildView(std::make_unique<views::Label>());
    title_label_->SetText(u"Temp Title Label for Glanceables");
  }

  int max_height = CalculateMaxTrayBubbleHeight();
  bubble_view_->SetMaxHeight(max_height);
  bubble_view_->ChangeAnchorAlignment(tray_->shelf()->alignment());
  bubble_view_->ChangeAnchorRect(tray_->shelf()->GetSystemTrayAnchorRect());
}

bool GlanceableTrayBubble::IsBubbleActive() const {
  return bubble_widget_->IsActive();
}

}  // namespace ash

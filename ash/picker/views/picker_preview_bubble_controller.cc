// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble_controller.h"

#include "ash/picker/views/picker_preview_bubble.h"
#include "base/check.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

PickerPreviewBubbleController::PickerPreviewBubbleController() = default;

PickerPreviewBubbleController::~PickerPreviewBubbleController() {
  CloseBubble();
}

void PickerPreviewBubbleController::ShowBubble(views::View* anchor_view) {
  if (bubble_view_ != nullptr) {
    return;
  }

  // Observe the destruction of the widget to keep `bubble_view_` from
  // dangling.
  CHECK(anchor_view);
  bubble_view_ = new PickerPreviewBubbleView(anchor_view);
  widget_observation_.Observe(bubble_view_->GetWidget());
}

void PickerPreviewBubbleController::CloseBubble() {
  if (bubble_view_ == nullptr) {
    return;
  }
  bubble_view_->Close();
  OnWidgetDestroying(bubble_view_->GetWidget());
}

void PickerPreviewBubbleController::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  bubble_view_ = nullptr;
}

views::View* PickerPreviewBubbleController::bubble_view_for_testing() const {
  return bubble_view_;
}

}  // namespace ash

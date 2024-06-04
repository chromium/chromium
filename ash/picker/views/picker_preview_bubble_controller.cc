// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble_controller.h"

#include "ash/picker/views/picker_preview_bubble.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/check.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

PickerPreviewBubbleController::PickerPreviewBubbleController() = default;

PickerPreviewBubbleController::~PickerPreviewBubbleController() {
  CloseBubble();
}

void PickerPreviewBubbleController::ShowBubble(
    HoldingSpaceImage* async_preview_image,
    views::View* anchor_view) {
  if (bubble_view_ != nullptr) {
    return;
  }

  // Observe the destruction of the widget to keep `bubble_view_` from
  // dangling.
  CHECK(anchor_view);
  bubble_view_ = new PickerPreviewBubbleView(anchor_view);
  async_preview_image_ = async_preview_image;
  bubble_view_->SetPreviewImage(
      ui::ImageModel::FromImageSkia(async_preview_image_->GetImageSkia()));
  // base::Unretained is safe here since `image_subscription_` is a member.
  // During destruction, `image_subscription_` will be destroyed before the
  // other members, so the callback is guaranteed to be safe.
  image_subscription_ = async_preview_image_->AddImageSkiaChangedCallback(
      base::BindRepeating(&PickerPreviewBubbleController::UpdateBubbleImage,
                          base::Unretained(this)));
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

  async_preview_image_ = nullptr;
}

PickerPreviewBubbleView*
PickerPreviewBubbleController::bubble_view_for_testing() const {
  return bubble_view_;
}

void PickerPreviewBubbleController::UpdateBubbleImage() {
  if (bubble_view_ != nullptr) {
    bubble_view_->SetPreviewImage(
        ui::ImageModel::FromImageSkia(async_preview_image_->GetImageSkia(
            PickerPreviewBubbleView::kPreviewImageSize)));
  }
}

}  // namespace ash

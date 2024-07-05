// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble_controller.h"

#include "ash/picker/views/picker_preview_bubble.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/check.h"
#include "base/location.h"
#include "base/time/time.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Duration to wait before showing the preview bubble when it is requested.
constexpr base::TimeDelta kShowBubbleDelay = base::Milliseconds(600);

}  // namespace

PickerPreviewBubbleController::PickerPreviewBubbleController() = default;

PickerPreviewBubbleController::~PickerPreviewBubbleController() {
  CloseBubble();
}

void PickerPreviewBubbleController::ShowBubbleAfterDelay(
    HoldingSpaceImage* async_preview_image,
    views::View* anchor_view) {
  CreateBubbleWidget(async_preview_image, anchor_view);
  show_bubble_timer_.Start(
      FROM_HERE, kShowBubbleDelay,
      base::BindOnce(&PickerPreviewBubbleController::ShowBubble,
                     weak_ptr_factory_.GetWeakPtr()));
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

void PickerPreviewBubbleController::ShowBubbleImmediatelyForTesting(
    HoldingSpaceImage* async_preview_image,
    views::View* anchor_view) {
  CreateBubbleWidget(async_preview_image, anchor_view);
  bubble_view_->GetWidget()->Show();
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

void PickerPreviewBubbleController::CreateBubbleWidget(
    HoldingSpaceImage* async_preview_image,
    views::View* anchor_view) {
  if (bubble_view_ != nullptr) {
    return;
  }

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

void PickerPreviewBubbleController::ShowBubble() {
  if (bubble_view_ != nullptr) {
    bubble_view_->GetWidget()->Show();
  }
}

}  // namespace ash

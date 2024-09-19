// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/facegaze_bubble_controller.h"

#include "ash/system/accessibility/facegaze_bubble_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

FaceGazeBubbleController::FaceGazeBubbleController() = default;

FaceGazeBubbleController::~FaceGazeBubbleController() {
  if (widget_ && !widget_->IsClosed()) {
    widget_->CloseNow();
  }
}

void FaceGazeBubbleController::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view != facegaze_bubble_view_) {
    return;
  }

  facegaze_bubble_view_->views::View::RemoveObserver(this);
  facegaze_bubble_view_ = nullptr;
  widget_ = nullptr;
}

void FaceGazeBubbleController::UpdateBubble(const std::u16string& text) {
  MaybeInitialize();
  Update(text);
  widget_->Show();
}

void FaceGazeBubbleController::MaybeInitialize() {
  if (widget_) {
    return;
  }

  facegaze_bubble_view_ = new FaceGazeBubbleView();
  facegaze_bubble_view_->views::View::AddObserver(this);

  widget_ =
      views::BubbleDialogDelegateView::CreateBubble(facegaze_bubble_view_);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kFaceGazeBubble);
}

void FaceGazeBubbleController::Update(const std::u16string& text) {
  if (!facegaze_bubble_view_) {
    return;
  }

  facegaze_bubble_view_->Update(text);

  // The bubble should be centered at the top of the screen.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  gfx::Size bubble_size = facegaze_bubble_view_->size();
  int center = (display_size.width() / 2) - (bubble_size.width() / 2);
  facegaze_bubble_view_->SetAnchorRect(gfx::Rect(center, 0, 1, 1));
}

}  // namespace ash

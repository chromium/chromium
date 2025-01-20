// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/facegaze_bubble_controller.h"

#include "ash/system/accessibility/facegaze_bubble_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "base/functional/bind.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
constexpr base::TimeDelta kShowTimeout = base::Seconds(1);
constexpr int kMarginFromTopDip = 8;
}  // namespace

FaceGazeBubbleController::FaceGazeBubbleController() = default;

FaceGazeBubbleController::~FaceGazeBubbleController() {
  show_timer_.Stop();
  if (widget_ && !widget_->IsClosed()) {
    widget_->CloseNow();
  }
}

void FaceGazeBubbleController::OnViewIsDeleting(views::View* observed_view) {
  if (observed_view != facegaze_bubble_view_) {
    return;
  }

  show_timer_.Stop();
  facegaze_bubble_view_->views::View::RemoveObserver(this);
  facegaze_bubble_view_ = nullptr;
  widget_ = nullptr;
}

void FaceGazeBubbleController::UpdateBubble(const std::u16string& text,
                                            bool is_warning) {
  MaybeInitialize();
  Update(text, is_warning);
  if (!show_timer_.IsRunning()) {
    widget_->Show();
  }
}

void FaceGazeBubbleController::MaybeInitialize() {
  if (widget_) {
    return;
  }

  facegaze_bubble_view_ = new FaceGazeBubbleView(base::BindRepeating(
      &FaceGazeBubbleController::OnMouseEntered, GetWeakPtr()));
  facegaze_bubble_view_->views::View::AddObserver(this);

  widget_ =
      views::BubbleDialogDelegateView::CreateBubble(facegaze_bubble_view_);
  CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
      widget_->GetNativeWindow(),
      CollisionDetectionUtils::RelativePriority::kFaceGazeBubble);
}

void FaceGazeBubbleController::Update(const std::u16string& text,
                                      bool is_warning) {
  if (!facegaze_bubble_view_) {
    return;
  }

  facegaze_bubble_view_->Update(text, is_warning);

  const gfx::Rect primary_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Size work_area_size = primary_work_area.size();
  const gfx::Size bubble_size = facegaze_bubble_view_->size();

  // The bubble should be centered at the top of the screen, factoring in other
  // UI elements such as the ChromeVox panel. Note that the work area may not
  // always start at (0, 0) so we need to factor in the starting point of the
  // work area.
  int center = (work_area_size.width() / 2) - (bubble_size.width() / 2) +
               primary_work_area.x();
  int top = primary_work_area.y() + kMarginFromTopDip;
  facegaze_bubble_view_->SetAnchorRect(gfx::Rect(center, top, 0, 0));
}

void FaceGazeBubbleController::OnMouseEntered() {
  widget_->Hide();
  show_timer_.Start(FROM_HERE, kShowTimeout,
                    base::BindRepeating(&FaceGazeBubbleController::OnShowTimer,
                                        GetWeakPtr()));
}

void FaceGazeBubbleController::OnShowTimer() {
  widget_->Show();
}

}  // namespace ash

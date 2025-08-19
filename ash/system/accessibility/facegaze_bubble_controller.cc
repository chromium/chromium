// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/facegaze_bubble_controller.h"

#include "ash/system/accessibility/facegaze_bubble_view.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "base/functional/bind.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
constexpr int kBubbleViewOutsets = 25;
constexpr int kMarginFromTopDip = 8;
constexpr base::TimeDelta kShowTimeout = base::Seconds(1);
}  // namespace

FaceGazeBubbleController::FaceGazeBubbleController(
    const base::RepeatingCallback<void()>& on_close_button_clicked)
    : on_close_button_clicked_(std::move(on_close_button_clicked)) {}

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

  facegaze_bubble_view_ = new FaceGazeBubbleView(
      base::BindRepeating(&FaceGazeBubbleController::OnMouseEntered,
                          GetWeakPtr()),
      base::BindRepeating(&FaceGazeBubbleController::OnCloseButtonClicked,
                          GetWeakPtr()));
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
      display::Screen::Get()->GetPrimaryDisplay().work_area();
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

void FaceGazeBubbleController::OnCloseButtonClicked(const ui::Event& event) {
  on_close_button_clicked_.Run();
}

void FaceGazeBubbleController::OnShowTimer() {
  gfx::Point cursor_location = display::Screen::Get()->GetCursorScreenPoint();
  // Expand the FaceGazeBubbleView bounds by 25 pixels in each direction.
  // This provides a cushion so that we don't show the UI when the user is
  // trying to click on an element that is a few pixels outside of the original
  // bounds.
  gfx::Rect scaled_bounds = facegaze_bubble_view_->GetBoundsInScreen();
  scaled_bounds.Outset(kBubbleViewOutsets);
  if (scaled_bounds.Contains(cursor_location)) {
    // Though we hide FaceGazeBubble view only if the main content is hovered,
    // we continue to hide it if the mouse is contained by the entire bounds of
    // the view. This is to allow users to click on elements occluded by
    // FaceGazeBubbleView.
    OnMouseEntered();
    return;
  }

  // If the mouse cursor isn't contained by the bubble, then we can show it.
  widget_->Show();
}

}  // namespace ash

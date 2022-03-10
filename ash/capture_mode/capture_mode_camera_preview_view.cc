// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_preview_view.h"

#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"

namespace ash {

namespace {

gfx::PointF GetEventScreenLocation(const ui::LocatedEvent& event) {
  return event.target()->GetScreenLocationF(event);
}

}  // namespace

CameraPreviewView::CameraPreviewView(
    CaptureModeCameraController* camera_controller)
    : camera_controller_(camera_controller) {
  SetPaintToLayer();
  // TODO: The solid color contents view will be replaced later by the view that
  // will render the video frams.
  SetBackground(views::CreateSolidBackground(gfx::kGoogleGrey700));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(capture_mode::kCameraPreviewSize.height() / 2.f));

  // TODO(crbug.com/1295325): Update this when implementing video frames
  // rendering.
  SetPreferredSize(capture_mode::kCameraPreviewSize);
}

CameraPreviewView::~CameraPreviewView() = default;

bool CameraPreviewView::OnMousePressed(const ui::MouseEvent& event) {
  camera_controller_->StartDraggingPreview(GetEventScreenLocation(event));
  return true;
}

bool CameraPreviewView::OnMouseDragged(const ui::MouseEvent& event) {
  camera_controller_->ContinueDraggingPreview(GetEventScreenLocation(event));
  return true;
}

void CameraPreviewView::OnMouseReleased(const ui::MouseEvent& event) {
  camera_controller_->EndDraggingPreview(GetEventScreenLocation(event),
                                         /*is_touch=*/false);
}

void CameraPreviewView::OnGestureEvent(ui::GestureEvent* event) {
  const gfx::PointF screen_location = GetEventScreenLocation(*event);

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      camera_controller_->StartDraggingPreview(screen_location);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      DCHECK(camera_controller_->is_drag_in_progress());
      camera_controller_->ContinueDraggingPreview(screen_location);
      break;
    case ui::ET_SCROLL_FLING_START:
      // TODO(conniekxu): Handle fling event.
      break;
    case ui::ET_GESTURE_SCROLL_END:
      DCHECK(camera_controller_->is_drag_in_progress());
      camera_controller_->EndDraggingPreview(screen_location,
                                             /*is_touch=*/true);
      break;
    case ui::ET_GESTURE_END:
      if (camera_controller_->is_drag_in_progress())
        camera_controller_->EndDraggingPreview(screen_location,
                                               /*is_touch=*/true);
      break;
    default:
      break;
  }

  event->StopPropagation();
  event->SetHandled();
}

BEGIN_METADATA(CameraPreviewView, views::View)
END_METADATA

}  // namespace ash

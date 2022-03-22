// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_preview_view.h"

#include "ash/capture_mode/capture_mode_button.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"

namespace ash {

namespace {

gfx::PointF GetEventScreenLocation(const ui::LocatedEvent& event) {
  return event.target()->GetScreenLocationF(event);
}

const gfx::VectorIcon& GetIconOfResizeButton(
    const bool is_camera_preview_collapsed) {
  return is_camera_preview_collapsed ? kCaptureModeCameraPreviewExpandIcon
                                     : kCaptureModeCameraPreviewCollapseIcon;
}

}  // namespace

CameraPreviewView::CameraPreviewView(
    CaptureModeCameraController* camera_controller,
    const gfx::Size& preview_view_preferred_size)
    : camera_controller_(camera_controller),
      resize_button_(AddChildView(std::make_unique<CaptureModeButton>(
          base::BindRepeating(&CameraPreviewView::OnResizeButtonPressed,
                              base::Unretained(this)),
          GetIconOfResizeButton(
              camera_controller_->is_camera_preview_collapsed())))) {
  SetPaintToLayer();
  // TODO: The solid color contents view will be replaced later by the view that
  // will render the video frams.
  SetBackground(views::CreateSolidBackground(gfx::kGoogleGrey700));

  // TODO(crbug.com/1295325): Update this when implementing video frames
  // rendering.
  SetPreferredSize(preview_view_preferred_size);

  resize_button_->SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80),
      resize_button_->GetPreferredSize().height() / 2.f));
  UpdateResizeButtonTooltip();
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

void CameraPreviewView::Layout() {
  const gfx::Size resize_button_size = resize_button_->GetPreferredSize();
  const gfx::Rect bounds(
      (width() - resize_button_size.width()) / 2.f,
      height() - resize_button_size.height() -
          capture_mode::kSpaceBetweenResizeButtonAndCameraPreview,
      resize_button_size.width(), resize_button_size.height());
  resize_button_->SetBoundsRect(bounds);

  GetWidget()->GetLayer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(height() / 2.f));
}

void CameraPreviewView::OnResizeButtonPressed() {
  camera_controller_->ToggleCameraPreviewSize();
  UpdateResizeButton();
}

void CameraPreviewView::UpdateResizeButton() {
  resize_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          GetIconOfResizeButton(
              camera_controller_->is_camera_preview_collapsed()),
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)));
  UpdateResizeButtonTooltip();
}

void CameraPreviewView::UpdateResizeButtonTooltip() {
  resize_button_->SetTooltipText(l10n_util::GetStringUTF16(
      camera_controller_->is_camera_preview_collapsed()
          ? IDS_ASH_SCREEN_CAPTURE_TOOLTIP_EXPAND_SELFIE_CAMERA
          : IDS_ASH_SCREEN_CAPTURE_TOOLTIP_COLLAPSE_SELFIE_CAMERA));
}

BEGIN_METADATA(CameraPreviewView, views::View)
END_METADATA

}  // namespace ash

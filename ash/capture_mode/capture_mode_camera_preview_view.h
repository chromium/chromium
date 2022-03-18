// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {

class CaptureModeCameraController;
class CaptureModeButton;

// A view that acts as the contents view of the camera preview widget. It will
// be responsible for painting the latest camera video frame inside its bounds.
class CameraPreviewView : public views::View {
 public:
  METADATA_HEADER(CameraPreviewView);

  CameraPreviewView(CaptureModeCameraController* camera_controller,
                    const gfx::Size& preview_view_preferred_size);
  CameraPreviewView(const CameraPreviewView&) = delete;
  CameraPreviewView& operator=(const CameraPreviewView&) = delete;
  ~CameraPreviewView() override;

  CaptureModeButton* resize_button_for_test() const { return resize_button_; }

 protected:
  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void Layout() override;

 private:
  // Called when the resize button is clicked or touched.
  void OnResizeButtonPressed();

  // Updates the icon of the `resize_button_` based on value of
  // `is_camera_preview_collapsed()` inferred from the `camera_controller`.
  void UpdateResizeButton();

  // Updates the tooltip of the `resize_button_`. The `resize_button_` can be an
  // expand button or collapse button.
  void UpdateResizeButtonTooltip();

  CaptureModeCameraController* const camera_controller_;
  CaptureModeButton* const resize_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_

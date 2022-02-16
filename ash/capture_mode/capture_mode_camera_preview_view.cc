// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_preview_view.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"

namespace ash {

CameraPreviewView::CameraPreviewView() {
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

BEGIN_METADATA(CameraPreviewView, views::View)
END_METADATA

}  // namespace ash

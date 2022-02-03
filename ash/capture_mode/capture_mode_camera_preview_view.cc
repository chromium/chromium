// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_camera_preview_view.h"

#include "ash/shell.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"

namespace ash {

namespace {

// The lower bound size of camera view.
constexpr gfx::Size kCameraPreviewWidgetSize{96, 96};

constexpr int kCameraPreviewMarginWithBounds = 200;

}  // namespace

CameraPreviewView::CameraPreviewView() {
  SetPaintToLayer();
  // TODO: The solid color contents view will be replaced later by the view that
  // will render the video frams.
  SetBackground(views::CreateSolidBackground(gfx::kGoogleGrey700));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCameraPreviewWidgetSize.height() / 2.f));
}

CameraPreviewView::~CameraPreviewView() = default;

// static
gfx::Rect CameraPreviewView::CalculateCameraPreviewWidgetBounds() {
  const auto root_bounds = Shell::GetPrimaryRootWindow()->GetBoundsInScreen();
  return gfx::Rect(
      root_bounds.bottom_right().x() - kCameraPreviewMarginWithBounds,
      root_bounds.bottom_right().y() - kCameraPreviewMarginWithBounds,
      kCameraPreviewWidgetSize.width(), kCameraPreviewWidgetSize.height());
}

BEGIN_METADATA(CameraPreviewView, views::View)
END_METADATA

}  // namespace ash
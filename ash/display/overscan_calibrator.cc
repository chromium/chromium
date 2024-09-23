// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/overscan_calibrator.h"

#include <stdint.h>

#include <limits>
#include <memory>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace ash {
namespace {

// The opacity for the arrows of the overscan calibration.
const float kArrowOpacity = 0.8;

// The height in pixel for the arrows to show the overscan calibration.
const int kCalibrationArrowHeight = 70;

// The gap between the boundary and calibration arrows.
const int kArrowGapWidth = 0;

// Draw the arrow for the overscan calibration to |canvas|.
void DrawTriangle(int x_offset,
                  int y_offset,
                  double rotation_degree,
                  gfx::Canvas* canvas) {
  // Draw triangular arrows.
  cc::PaintFlags content_flags;
  content_flags.setStyle(cc::PaintFlags::kFill_Style);
  content_flags.setColor(SkColorSetA(
      SK_ColorBLACK, std::numeric_limits<uint8_t>::max() * kArrowOpacity));
  cc::PaintFlags border_flags;
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setColor(SkColorSetA(
      SK_ColorWHITE, std::numeric_limits<uint8_t>::max() * kArrowOpacity));

  SkPath base_path;
  base_path.moveTo(0, 0);
  base_path.lineTo(SkIntToScalar(-kCalibrationArrowHeight),
                   SkIntToScalar(-kCalibrationArrowHeight));
  base_path.lineTo(SkIntToScalar(kCalibrationArrowHeight),
                   SkIntToScalar(-kCalibrationArrowHeight));
  base_path.close();

  SkPath path;
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(rotation_degree);
  gfx::Transform move_transform;
  move_transform.Translate(x_offset, y_offset);
  rotate_transform.PostConcat(move_transform);
  base_path.transform(gfx::TransformToFlattenedSkMatrix(rotate_transform),
                      &path);

  canvas->DrawPath(path, content_flags);
  canvas->DrawPath(path, border_flags);
}

gfx::Insets RotateInsets(display::Display::Rotation rotation,
                         gfx::Insets&& insets) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return insets;
    case display::Display::ROTATE_90:
      return gfx::Insets::TLBR(insets.right(), insets.top(), insets.left(),
                               insets.bottom());
    case display::Display::ROTATE_180:
      return gfx::Insets::TLBR(insets.bottom(), insets.right(), insets.top(),
                               insets.left());
    case display::Display::ROTATE_270:
      return gfx::Insets::TLBR(insets.left(), insets.bottom(), insets.right(),
                               insets.top());
  }
  NOTREACHED();
}

gfx::Insets ConvertToDisplay(const display::Display& display,
                             const gfx::Insets& insets) {
  display::ManagedDisplayInfo info =
      ash::Shell::Get()->display_manager()->GetDisplayInfo(display.id());
  return RotateInsets(
      display.rotation(),
      gfx::ScaleToFlooredInsets(
          insets, info.device_scale_factor() / display.device_scale_factor()));
}

gfx::Insets ConvertToHost(const display::Display& display,
                          const gfx::Insets& insets) {
  display::ManagedDisplayInfo info =
      ash::Shell::Get()->display_manager()->GetDisplayInfo(display.id());
  display::Display::Rotation inverted_rotation =
      static_cast<display::Display::Rotation>(
          (4 - static_cast<int>(display.rotation())) % 4);
  return RotateInsets(
      inverted_rotation,
      gfx::ScaleToFlooredInsets(
          insets, display.device_scale_factor() / info.device_scale_factor()));
}

}  // namespace

OverscanCalibrator::OverscanCalibrator(const display::Display& target_display,
                                       const gfx::Insets& initial_insets)
    : display_(target_display),
      insets_(ConvertToDisplay(display_, initial_insets)),
      initial_insets_(initial_insets),
      committed_(false) {
  // Undo the overscan calibration temporarily so that the user can see
  // dark boundary and current overscan region.
  Shell::Get()->window_tree_host_manager()->SetOverscanInsets(display_.id(),
                                                              gfx::Insets());
  UpdateUILayer();
}

OverscanCalibrator::~OverscanCalibrator() {
  // Overscan calibration has finished without commit, so the display has to
  // be the original offset.
  if (!committed_) {
    Shell::Get()->window_tree_host_manager()->SetOverscanInsets(
        display_.id(), initial_insets_);
  }
}

void OverscanCalibrator::Commit() {
  Shell::Get()->window_tree_host_manager()->SetOverscanInsets(
      display_.id(), ConvertToHost(display_, insets_));
  committed_ = true;
}

void OverscanCalibrator::Reset() {
  insets_ = ConvertToDisplay(display_, initial_insets_);
  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::UpdateInsets(const gfx::Insets& insets) {
  insets_ = gfx::Insets::TLBR(
      std::max(insets.top(), 0), std::max(insets.left(), 0),
      std::max(insets.bottom(), 0), std::max(insets.right(), 0));
  calibration_layer_->SchedulePaint(calibration_layer_->bounds());
}

void OverscanCalibrator::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, calibration_layer_->size());
  gfx::Rect full_bounds = calibration_layer_->bounds();
  gfx::Rect inner_bounds = full_bounds;
  inner_bounds.Inset(insets_);
  recorder.canvas()->FillRect(full_bounds, SK_ColorBLACK);
  recorder.canvas()->FillRect(inner_bounds, SK_ColorTRANSPARENT,
                              SkBlendMode::kClear);

  gfx::Point center = inner_bounds.CenterPoint();
  int vertical_offset = inner_bounds.height() / 2 - kArrowGapWidth;
  int horizontal_offset = inner_bounds.width() / 2 - kArrowGapWidth;

  gfx::Canvas* canvas = recorder.canvas();
  DrawTriangle(center.x(), center.y() + vertical_offset, 0, canvas);
  DrawTriangle(center.x(), center.y() - vertical_offset, 180, canvas);
  DrawTriangle(center.x() - horizontal_offset, center.y(), 90, canvas);
  DrawTriangle(center.x() + horizontal_offset, center.y(), -90, canvas);
}

void OverscanCalibrator::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void OverscanCalibrator::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (display_.id() != display.id() || committed_)
    return;
  display_ = display;
  UpdateUILayer();
  Reset();
}

void OverscanCalibrator::UpdateUILayer() {
  display::ManagedDisplayInfo info =
      Shell::Get()->display_manager()->GetDisplayInfo(display_.id());

  aura::Window* root = Shell::GetRootWindowForDisplayId(display_.id());
  ui::Layer* parent_layer =
      Shell::GetContainer(root, kShellWindowId_OverlayContainer)->layer();
  calibration_layer_ = std::make_unique<ui::Layer>();
  calibration_layer_->SetOpacity(0.5f);
  calibration_layer_->SetBounds(parent_layer->bounds());
  calibration_layer_->set_delegate(this);
  parent_layer->Add(calibration_layer_.get());
}

}  // namespace ash

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_ring.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"

namespace ash {
namespace {

// Appearance.
constexpr float kStrokeWidth = 2.f;
constexpr float kTrackOpacity = 0.3f;

// Helpers ---------------------------------------------------------------------

// Returns the sweep angle to represent progress of the specified `item`.
// NOTE: This method may only be called if `item` has determinate progress.
float CalculateSweepAngle(const HoldingSpaceItem* item) {
  DCHECK(item->IsInProgress());
  DCHECK(item->progress().has_value());
  return 360.f * item->progress().value();
}

}  // namespace

// HoldingSpaceProgressRing ----------------------------------------------------

HoldingSpaceProgressRing::HoldingSpaceProgressRing(
    const HoldingSpaceItem* item,
    bool use_light_mode_as_default)
    : ui::LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED)),
      item_(item),
      use_light_mode_as_default_(use_light_mode_as_default) {
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(false);
  model_observation_.Observe(HoldingSpaceController::Get()->model());
}

HoldingSpaceProgressRing::~HoldingSpaceProgressRing() = default;

void HoldingSpaceProgressRing::InvalidateLayer() {
  layer()->SchedulePaint(gfx::Rect(layer()->size()));
}

void HoldingSpaceProgressRing::OnDeviceScaleFactorChanged(float old_scale,
                                                          float new_scale) {
  InvalidateLayer();
}

// TODO(crbug.com/1184438): Handle indeterminate progress.
void HoldingSpaceProgressRing::OnPaintLayer(const ui::PaintContext& context) {
  if (!item_ || !item_->IsInProgress() || !item_->progress().has_value())
    return;

  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();

  // The `canvas` should be flipped for RTL.
  gfx::ScopedCanvas scoped_canvas(recorder.canvas());
  scoped_canvas.FlipIfRTL(layer()->size().width());

  gfx::Rect bounds(layer()->size());
  bounds.Inset(gfx::Insets(std::ceil(kStrokeWidth / 2.f)));

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStrokeCap(cc::PaintFlags::Cap::kRound_Cap);
  flags.setStrokeWidth(kStrokeWidth);
  flags.setStyle(cc::PaintFlags::Style::kStroke_Style);

  std::unique_ptr<ScopedLightModeAsDefault> scoped_light_mode_as_default =
      use_light_mode_as_default_ ? std::make_unique<ScopedLightModeAsDefault>()
                                 : nullptr;

  const SkColor color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  // Track.
  flags.setColor(SkColorSetA(color, 0xFF * kTrackOpacity));
  canvas->DrawCircle(gfx::PointF(bounds.CenterPoint()),
                     std::min(bounds.height(), bounds.width()) / 2.f, flags);

  // Ring.
  flags.setColor(color);
  canvas->DrawPath(
      SkPath().arcTo(/*oval=*/gfx::RectToSkRect(bounds), /*start_angle=*/-90,
                     /*sweep_angle=*/CalculateSweepAngle(item_),
                     /*forceMoveTo=*/false),
      flags);
}

void HoldingSpaceProgressRing::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item) {
  if (item_ == item)
    InvalidateLayer();
}

void HoldingSpaceProgressRing::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  for (const HoldingSpaceItem* item : items) {
    if (item_ == item) {
      item_ = nullptr;
      return;
    }
  }
}

}  // namespace ash
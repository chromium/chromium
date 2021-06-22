// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_ring.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "base/scoped_observation.h"
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

// Returns the sweep angle to use to represent the specified `progress`.
// NOTE: The specified `progress` must be >= `0.f` and < `1.f`.
float CalculateSweepAngle(float progress) {
  DCHECK_GE(progress, 0.f);
  DCHECK_LT(progress, 1.f);
  return 360.f * progress;
}

// HoldingSpaceItemProgressRing ------------------------------------------------

// A class owning a `ui::Layer` which paints a ring to indicate progress of an
// associated holding space `item_`. NOTE: The owned `layer()` is not painted if
// the associated `item_` is not in progress.
class HoldingSpaceItemProgressRing : public HoldingSpaceProgressRing,
                                     public HoldingSpaceModelObserver {
 public:
  HoldingSpaceItemProgressRing(const HoldingSpaceItem* item,
                               bool use_light_mode_as_default)
      : HoldingSpaceProgressRing(use_light_mode_as_default), item_(item) {
    model_observation_.Observe(HoldingSpaceController::Get()->model());
  }

 private:
  // HoldingSpaceProgressRing:
  absl::optional<float> GetProgress() const override {
    // If `item_` is `nullptr` it is being destroyed. Return `1.f` in that case
    // so that no progress ring will be painted.
    return item_ ? item_->progress() : 1.f;
  }

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) override {
    if (item_ == item)
      InvalidateLayer();
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items) {
      if (item_ == item) {
        item_ = nullptr;
        return;
      }
    }
  }

  // The associated holding space `item` for which to indicate progress.
  // NOTE: May temporarily be `nullptr` during the `item`s destruction sequence.
  const HoldingSpaceItem* item_ = nullptr;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observation_{this};
};

}  // namespace

// HoldingSpaceProgressRing ----------------------------------------------------

HoldingSpaceProgressRing::HoldingSpaceProgressRing(
    bool use_light_mode_as_default)
    : ui::LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED)),
      use_light_mode_as_default_(use_light_mode_as_default) {
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(false);
}

HoldingSpaceProgressRing::~HoldingSpaceProgressRing() = default;

// static
std::unique_ptr<HoldingSpaceProgressRing>
HoldingSpaceProgressRing::CreateForItem(const HoldingSpaceItem* item,
                                        bool use_light_mode_as_default) {
  return std::make_unique<HoldingSpaceItemProgressRing>(
      item, use_light_mode_as_default);
}

void HoldingSpaceProgressRing::InvalidateLayer() {
  layer()->SchedulePaint(gfx::Rect(layer()->size()));
}

void HoldingSpaceProgressRing::OnDeviceScaleFactorChanged(float old_scale,
                                                          float new_scale) {
  InvalidateLayer();
}

// TODO(crbug.com/1184438): Handle indeterminate progress.
void HoldingSpaceProgressRing::OnPaintLayer(const ui::PaintContext& context) {
  const absl::optional<float> progress(GetProgress());
  if (!progress.has_value() || progress.value() == 1.f)
    return;

  DCHECK_GE(progress.value(), 0.f);
  DCHECK_LT(progress.value(), 1.f);

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
                     /*sweep_angle=*/CalculateSweepAngle(progress.value()),
                     /*forceMoveTo=*/false),
      flags);
}

}  // namespace ash
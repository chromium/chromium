// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_ring.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/style/ash_color_provider.h"
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

// HoldingSpaceControllerProgressRing ------------------------------------------

// A class owning a `ui::Layer` which paints a ring to indicate progress of all
// items in the model attached to its associated holding space `controller_`.
// NOTE: The owned `layer()` is not painted if there are no items in progress.
class HoldingSpaceControllerProgressRing
    : public HoldingSpaceProgressRing,
      public HoldingSpaceControllerObserver,
      public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceControllerProgressRing(
      HoldingSpaceController* controller)
      : controller_(controller) {
    controller_observation_.Observe(controller_);
    if (controller_->model())
      OnHoldingSpaceModelAttached(controller_->model());
  }

 private:
  // HoldingSpaceProgressRing:
  absl::optional<float> GetProgress() const override {
    // If there is no `model` attached, then there are no in-progress holding
    // space items. Return `1.f` to prevent the progress ring from painting.
    const HoldingSpaceModel* model = controller_->model();
    if (!model)
      return 1.f;

    // TODO(crbug.com/1184438): The below progress calculation is a temporary
    // approximation until a more accurate cumulative progress calculation is
    // implemented in a follow up CL.
    float cumulative_progress = 0.f;
    int number_of_in_progress_items = 0;

    // Iterate over all holding space items.
    for (const auto& item : model->items()) {
      // Ignore any holding space items that are not yet initialized, since
      // they are not visible to the user, or items that are not in-progress,
      // since they do not contribute to cumulative progress.
      if (!item->IsInitialized() || !item->IsInProgress())
        continue;

      // If any holding space `item` has indeterminate progress, then cumulative
      // progress is also indeterminate.
      if (!item->progress().has_value())
        return absl::nullopt;

      // Update incremental tracking of cumulative progress.
      cumulative_progress += item->progress().value();
      ++number_of_in_progress_items;
    }

    // If there are no in-progress holding space items, return `1.f` to prevent
    // the progress ring from painting.
    if (number_of_in_progress_items == 0)
      return 1.f;

    // Return the average progress as a temporary approximation until a more
    // accurate cumulative progress can be calculated.
    return cumulative_progress / number_of_in_progress_items;
  }

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override {
    model_observation_.Observe(model);
    InvalidateLayer();
  }

  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override {
    model_observation_.Reset();
    InvalidateLayer();
  }

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items) {
      if (item->IsInitialized() && item->IsInProgress()) {
        InvalidateLayer();
        return;
      }
    }
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items) {
      if (item->IsInitialized() && item->IsInProgress()) {
        InvalidateLayer();
        return;
      }
    }
  }

  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) override {
    if (item->IsInitialized())
      InvalidateLayer();
  }

  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override {
    if (item->IsInProgress())
      InvalidateLayer();
  }

  // The associated holding space `controller_` for which to indicate progress
  // of all holding space items in its attached model.
  HoldingSpaceController* const controller_;

  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      controller_observation_{this};

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observation_{this};
};

// HoldingSpaceItemProgressRing ------------------------------------------------

// A class owning a `ui::Layer` which paints a ring to indicate progress of its
// associated holding space `item_`. NOTE: The owned `layer()` is not painted if
// the associated `item_` is not in progress.
class HoldingSpaceItemProgressRing : public HoldingSpaceProgressRing,
                                     public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceItemProgressRing(const HoldingSpaceItem* item)
      : item_(item) {
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

HoldingSpaceProgressRing::HoldingSpaceProgressRing()
    : ui::LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED)) {
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(false);
}

HoldingSpaceProgressRing::~HoldingSpaceProgressRing() = default;

// static
std::unique_ptr<HoldingSpaceProgressRing>
HoldingSpaceProgressRing::CreateForController(
    HoldingSpaceController* controller) {
  return std::make_unique<HoldingSpaceControllerProgressRing>(controller);
}

// static
std::unique_ptr<HoldingSpaceProgressRing>
HoldingSpaceProgressRing::CreateForItem(const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemProgressRing>(item);
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
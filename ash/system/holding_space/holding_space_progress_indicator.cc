// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_progress_indicator.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_progress_ring_indeterminate_animation.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkPathMeasure.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"

namespace ash {
namespace {

// Appearance.
constexpr float kInnerIconSizeScaleFactor = 14.f / 28.f;
constexpr float kOuterRingOpacity = 0.6f;
constexpr float kInnerRingStrokeWidthScaleFactor = 1.5f / 28.f;
constexpr float kOuterRingStrokeWidth = 2.f;
constexpr float kOuterRingStrokeWidthScaleFactor = 4.f / 28.f;
constexpr float kOuterRingTrackOpacity = 0.3f;

// Helpers ---------------------------------------------------------------------

// Returns the segment of the specified `path` between `start` and `end`.
// NOTE: It is required that: `0.f` <= `start` <= `end` <= `1.f`.
SkPath CreatePathSegment(const SkPath& path, float start, float end) {
  DCHECK_LE(0.f, start);
  DCHECK_LE(start, end);
  DCHECK_LE(end, 1.f);

  SkPathMeasure measure(path, /*force_closed=*/false);
  start *= measure.getLength();
  end *= measure.getLength();

  SkPath path_segment;
  measure.getSegment(start, end, &path_segment, /*start_with_move_to=*/true);

  return path_segment;
}

// Returns a rounded rect path from the specified `rect` and `corner_radius`.
// NOTE: Unlike a typical rounded rect which starts from the *top-left* corner
// and proceeds clockwise, the rounded rect returned by this method starts at
// the *top-center*. This is a subtle but important detail as calling
// `CreatePathSegment()` with a path created from this method will treat
// *top-center* as the start point, as is needed when painting progress.
SkPath CreateRoundedRectPath(const gfx::RectF& rect, float corner_radius) {
  // Top center.
  SkPoint top_center(SkPoint::Make(rect.width() / 2.f, 0.f));

  // Top right.
  SkPoint top_right(SkPoint::Make(rect.width(), 0.f));
  SkPoint top_right_end(top_right);
  top_right_end.offset(0.f, corner_radius);

  // Bottom right.
  SkPoint bottom_right(SkPoint::Make(rect.width(), rect.height()));
  SkPoint bottom_right_end(bottom_right);
  bottom_right_end.offset(-corner_radius, 0.f);

  // Bottom left.
  SkPoint bottom_left(SkPoint::Make(0.f, rect.height()));
  SkPoint bottom_left_end(bottom_left);
  bottom_left_end.offset(0.f, -corner_radius);

  // Top left.
  SkPoint top_left(SkPoint::Make(0.f, 0.f));
  SkPoint top_left_end(top_left);
  top_left_end.offset(corner_radius, 0.f);

  // Build path in the order specified above.
  return SkPathBuilder()
      .moveTo(top_center)
      .arcTo(top_right, top_right_end, corner_radius)
      .arcTo(bottom_right, bottom_right_end, corner_radius)
      .arcTo(bottom_left, bottom_left_end, corner_radius)
      .arcTo(top_left, top_left_end, corner_radius)
      .close()
      .offset(rect.x(), rect.y())
      .detach();
}

// Returns the size for the inner icon given `layer` dimensions.
// NOTE: this method should only be called when v2 animations are enabled.
float GetInnerIconSize(const ui::Layer* layer) {
  DCHECK(features::IsHoldingSpaceInProgressAnimationV2Enabled());
  const gfx::Size& size = layer->size();
  return kInnerIconSizeScaleFactor * std::min(size.width(), size.height());
}

// Returns the stroke width for the inner icon given `layer` dimensions.
// NOTE: this method should only be called when v2 animations are enabled.
float GetInnerRingStrokeWidth(const ui::Layer* layer) {
  DCHECK(features::IsHoldingSpaceInProgressAnimationV2Enabled());
  const gfx::Size& size = layer->size();
  return kInnerRingStrokeWidthScaleFactor *
         std::min(size.width(), size.height());
}

// Returns the opacity for the outer ring given the current `progress`.
float GetOuterRingOpacity(const absl::optional<float>& progress) {
  return features::IsHoldingSpaceInProgressAnimationV2Enabled() &&
                 progress != HoldingSpaceProgressIndicator::kProgressComplete
             ? kOuterRingOpacity
             : 1.f;
}

// Returns the stroke width for the outer ring given `layer` dimensions and
// the current `progress`.
float GetOuterRingStrokeWidth(const ui::Layer* layer,
                              const absl::optional<float>& progress) {
  if (features::IsHoldingSpaceInProgressAnimationV2Enabled() &&
      progress != HoldingSpaceProgressIndicator::kProgressComplete) {
    const gfx::Size& size = layer->size();
    return kOuterRingStrokeWidthScaleFactor *
           std::min(size.width(), size.height());
  }
  return kOuterRingStrokeWidth;
}

// Returns the stroke cap.
cc::PaintFlags::Cap GetStrokeCap() {
  return features::IsHoldingSpaceInProgressAnimationV2Enabled()
             ? cc::PaintFlags::Cap::kDefault_Cap
             : cc::PaintFlags::Cap::kRound_Cap;
}

// HoldingSpaceControllerProgressIndicator -------------------------------------

// A class owning a `ui::Layer` which paints indication of progress for all
// items in the model attached to its associated holding space `controller_`.
// NOTE: The owned `layer()` is not painted if there are no items in progress.
class HoldingSpaceControllerProgressIndicator
    : public HoldingSpaceProgressIndicator,
      public HoldingSpaceControllerObserver,
      public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceControllerProgressIndicator(
      HoldingSpaceController* controller)
      : HoldingSpaceProgressIndicator(/*animation_key=*/controller),
        controller_(controller) {
    controller_observation_.Observe(controller_);
    if (controller_->model())
      OnHoldingSpaceModelAttached(controller_->model());
  }

 private:
  // HoldingSpaceProgressIndicator:
  absl::optional<float> CalculateProgress() const override {
    // If there is no `model` attached, then there are no in-progress holding
    // space items. Do not paint the progress indication.
    const HoldingSpaceModel* model = controller_->model();
    if (!model)
      return kProgressComplete;

    HoldingSpaceProgress cumulative_progress;

    // Iterate over all holding space items.
    for (const auto& item : model->items()) {
      // Ignore any holding space items that are not yet initialized, since
      // they are not visible to the user, or items that are not visibly
      // in-progress, since they do not contribute to `cumulative_progress`.
      if (item->IsInitialized() && !item->progress().IsHidden() &&
          !item->progress().IsComplete()) {
        cumulative_progress += item->progress();
      }
    }

    return cumulative_progress.GetValue();
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
      if (item->IsInitialized() && !item->progress().IsComplete()) {
        InvalidateLayer();
        return;
      }
    }
  }

  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override {
    for (const HoldingSpaceItem* item : items) {
      if (item->IsInitialized() && !item->progress().IsComplete()) {
        InvalidateLayer();
        return;
      }
    }
  }

  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                 uint32_t updated_fields) override {
    if (item->IsInitialized())
      InvalidateLayer();
  }

  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override {
    if (!item->progress().IsComplete())
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

// HoldingSpaceItemProgressIndicator -------------------------------------------

// A class owning a `ui::Layer` which paints indication of progress for its
// associated holding space `item_`. NOTE: The owned `layer()` is not painted if
// the associated `item_` is not in progress.
class HoldingSpaceItemProgressIndicator : public HoldingSpaceProgressIndicator,
                                          public HoldingSpaceModelObserver {
 public:
  explicit HoldingSpaceItemProgressIndicator(const HoldingSpaceItem* item)
      : HoldingSpaceProgressIndicator(/*animation_key=*/item), item_(item) {
    model_observation_.Observe(HoldingSpaceController::Get()->model());
  }

 private:
  // HoldingSpaceProgressIndicator:
  absl::optional<float> CalculateProgress() const override {
    // If `item_` is `nullptr` it is being destroyed. Ensure the progress
    // indication is not painted in this case. Similarly, ensure the progress
    // indication is not painted when progress is hidden.
    return item_ && !item_->progress().IsHidden() ? item_->progress().GetValue()
                                                  : kProgressComplete;
  }

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                 uint32_t updated_fields) override {
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

// HoldingSpaceProgressIndicator -----------------------------------------------

constexpr char HoldingSpaceProgressIndicator::kClassName[];
constexpr float HoldingSpaceProgressIndicator::kProgressComplete;

HoldingSpaceProgressIndicator::HoldingSpaceProgressIndicator(
    const void* animation_key)
    : ui::LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED)),
      animation_key_(animation_key) {
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetName(kClassName);

  HoldingSpaceAnimationRegistry* animation_registry =
      HoldingSpaceAnimationRegistry::GetInstance();

  // Register to be notified of changes to the ring animation associated with
  // this progress indicator's `animation_key_`. Note that it is safe to use a
  // raw pointer here since `this` owns the subscription.
  ring_animation_changed_subscription_ =
      animation_registry->AddProgressRingAnimationChangedCallbackForKey(
          animation_key_,
          base::BindRepeating(
              &HoldingSpaceProgressIndicator::OnProgressRingAnimationChanged,
              base::Unretained(this)));

  // If an `animation` is already registered, perform additional initialization.
  HoldingSpaceProgressRingAnimation* animation =
      animation_registry->GetProgressRingAnimationForKey(animation_key_);
  if (animation)
    OnProgressRingAnimationChanged(animation);
}

HoldingSpaceProgressIndicator::~HoldingSpaceProgressIndicator() = default;

// static
std::unique_ptr<HoldingSpaceProgressIndicator>
HoldingSpaceProgressIndicator::CreateForController(
    HoldingSpaceController* controller) {
  return std::make_unique<HoldingSpaceControllerProgressIndicator>(controller);
}

// static
std::unique_ptr<HoldingSpaceProgressIndicator>
HoldingSpaceProgressIndicator::CreateForItem(const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemProgressIndicator>(item);
}

void HoldingSpaceProgressIndicator::InvalidateLayer() {
  layer()->SchedulePaint(gfx::Rect(layer()->size()));
}

void HoldingSpaceProgressIndicator::SetInnerIconVisible(bool visible) {
  if (inner_icon_visible_ == visible)
    return;

  inner_icon_visible_ = visible;

  // It's not necessary to invalidate the `layer()` if progress is complete
  // since the inner icon is only painted while progress is incomplete.
  if (progress_ != kProgressComplete)
    InvalidateLayer();
}

void HoldingSpaceProgressIndicator::OnDeviceScaleFactorChanged(
    float old_scale,
    float new_scale) {
  InvalidateLayer();
}

void HoldingSpaceProgressIndicator::OnPaintLayer(
    const ui::PaintContext& context) {
  // Look up the associated `animation` (if one exists).
  HoldingSpaceProgressRingAnimation* ring_animation =
      HoldingSpaceAnimationRegistry::GetInstance()
          ->GetProgressRingAnimationForKey(animation_key_);

  // Unless `this` is animating, nothing will paint if `progress_` is complete.
  if (progress_ == kProgressComplete && !ring_animation)
    return;

  float start, end, opacity;
  if (ring_animation) {
    start = ring_animation->start_position();
    end = ring_animation->end_position();
    opacity = ring_animation->opacity();
  } else {
    start = 0.f;
    end = progress_.value();
    opacity = 1.f;
  }

  DCHECK_GE(start, 0.f);
  DCHECK_LE(start, 1.f);
  DCHECK_GE(end, 0.f);
  DCHECK_LE(end, 1.f);
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();

  // The `canvas` should be flipped for RTL.
  gfx::ScopedCanvas scoped_canvas(recorder.canvas());
  scoped_canvas.FlipIfRTL(layer()->size().width());

  float outer_ring_stroke_width = GetOuterRingStrokeWidth(layer(), progress_);
  gfx::RectF bounds(gfx::SizeF(layer()->size()));
  bounds.Inset(gfx::InsetsF(outer_ring_stroke_width / 2.f));
  SkPath path(CreateRoundedRectPath(
      bounds, /*radius=*/std::min(bounds.width(), bounds.height()) / 2.f));

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStrokeCap(GetStrokeCap());
  flags.setStrokeWidth(outer_ring_stroke_width);
  flags.setStyle(cc::PaintFlags::Style::kStroke_Style);

  const SkColor color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  // Outer ring track.
  if (!features::IsHoldingSpaceInProgressAnimationV2Enabled()) {
    flags.setColor(
        SkColorSetA(color, SK_AlphaOPAQUE * kOuterRingTrackOpacity * opacity));
    canvas->DrawPath(path, flags);
  }

  // Outer ring.
  flags.setColor(SkColorSetA(
      color, SK_AlphaOPAQUE * GetOuterRingOpacity(progress_) * opacity));
  if (start <= end) {
    // If `start` <= `end`, only a single path segment is necessary.
    canvas->DrawPath(CreatePathSegment(path, start, end), flags);
  } else {
    // If `start` > `end`, two path segments are used to give the illusion of a
    // single progress ring. This works around limitations of `SkPathMeasure`
    // which require that `start` be <= `end`.
    canvas->DrawPath(CreatePathSegment(path, start, 1.f), flags);
    canvas->DrawPath(CreatePathSegment(path, 0.f, end), flags);
  }

  // The inner ring and inner icon are only present in v2.
  if (!features::IsHoldingSpaceInProgressAnimationV2Enabled())
    return;

  // The inner ring and inner icon should be absent once progress completes.
  // This would occur if the progress ring is animating post completion.
  if (progress_ == kProgressComplete)
    return;

  float inner_ring_stroke_width = GetInnerRingStrokeWidth(layer());
  bounds.Inset(
      gfx::InsetsF((outer_ring_stroke_width + inner_ring_stroke_width) / 2.f));
  path = CreateRoundedRectPath(
      bounds, /*radius=*/std::min(bounds.width(), bounds.height()) / 2.f);

  // Inner ring.
  flags.setColor(color);
  flags.setStrokeWidth(inner_ring_stroke_width);
  canvas->DrawPath(path, flags);

  if (inner_icon_visible_) {
    float inner_icon_size = GetInnerIconSize(layer());
    gfx::Rect inner_icon_bounds(layer()->size());
    inner_icon_bounds.ClampToCenteredSize(
        gfx::Size(inner_icon_size, inner_icon_size));

    // Inner icon.
    canvas->Translate(
        gfx::Vector2d(inner_icon_bounds.x(), inner_icon_bounds.y()));
    gfx::PaintVectorIcon(canvas, kHoldingSpaceDownloadIcon, inner_icon_size,
                         color);
  }
}

void HoldingSpaceProgressIndicator::UpdateVisualState() {
  // Cache `progress_`.
  progress_ = CalculateProgress();
  if (progress_.has_value()) {
    DCHECK_GE(progress_.value(), 0.f);
    DCHECK_LE(progress_.value(), 1.f);
  }
}

void HoldingSpaceProgressIndicator::OnProgressRingAnimationChanged(
    HoldingSpaceProgressRingAnimation* animation) {
  // Trigger repaint of this progress indicator on `animation` updates. Note
  // that it is safe to use a raw pointer here since `this` owns the
  // subscription.
  if (animation) {
    ring_animation_updated_subscription_ =
        animation->AddAnimationUpdatedCallback(
            base::BindRepeating(&HoldingSpaceProgressIndicator::InvalidateLayer,
                                base::Unretained(this)));
  }
  InvalidateLayer();
}

}  // namespace ash

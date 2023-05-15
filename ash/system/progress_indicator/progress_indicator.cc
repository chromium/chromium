// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/progress_indicator/progress_indicator.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/progress_indicator/progress_icon_animation.h"
#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
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
  const gfx::Size& size = layer->size();
  return kInnerIconSizeScaleFactor * std::min(size.width(), size.height());
}

// Returns the stroke width for the inner icon given `layer` dimensions.
// NOTE: this method should only be called when v2 animations are enabled.
float GetInnerRingStrokeWidth(const ui::Layer* layer) {
  const gfx::Size& size = layer->size();
  return kInnerRingStrokeWidthScaleFactor *
         std::min(size.width(), size.height());
}

// Returns the opacity for the outer ring given the current `progress`.
float GetOuterRingOpacity(const absl::optional<float>& progress) {
  return progress != ProgressIndicator::kProgressComplete ? kOuterRingOpacity
                                                          : 1.f;
}

// Returns the stroke width for the outer ring given `layer` dimensions and
// the current `progress`.
float GetOuterRingStrokeWidth(const ui::Layer* layer,
                              const absl::optional<float>& progress) {
  if (progress != ProgressIndicator::kProgressComplete) {
    const gfx::Size& size = layer->size();
    return kOuterRingStrokeWidthScaleFactor *
           std::min(size.width(), size.height());
  }
  return kOuterRingStrokeWidth;
}

// DefaultProgressIndicatorAnimationRegistry -----------------------------------

// A default implementation of `ProgressIndicatorAnimationRegistry` which is
// associated with a single `ProgressIndicator` and manage progress animations
// as needed.
class DefaultProgressIndicatorAnimationRegistry
    : public ProgressIndicatorAnimationRegistry {
 public:
  DefaultProgressIndicatorAnimationRegistry() = default;
  DefaultProgressIndicatorAnimationRegistry(
      const DefaultProgressIndicatorAnimationRegistry&) = delete;
  DefaultProgressIndicatorAnimationRegistry& operator=(
      const DefaultProgressIndicatorAnimationRegistry&) = delete;
  ~DefaultProgressIndicatorAnimationRegistry() = default;

  // Sets the `progress_indicator` for which this registry manages animations.
  // NOTE: This method may be called only once.
  void SetProgressIndicator(ProgressIndicator* progress_indicator) {
    DCHECK(progress_indicator);
    DCHECK(!progress_indicator_);
    progress_indicator_ = progress_indicator;
    progress_changed_subscription_ =
        progress_indicator_->AddProgressChangedCallback(base::BindRepeating(
            &DefaultProgressIndicatorAnimationRegistry::OnProgressChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Invoked on changes to `progress_indicator_` progress.
  void OnProgressChanged() {
    const absl::optional<float>& progress = progress_indicator_->progress();
    if (!progress.has_value()) {
      // Progress is indeterminate.
      EnsureProgressIconAnimation();
      EnsureProgressRingAnimationOfType(
          ProgressRingAnimation::Type::kIndeterminate);
    } else if (progress != ProgressIndicator::kProgressComplete) {
      // Progress is determinate.
      EnsureProgressIconAnimation();
      EraseProgressRingAnimation();
    } else if (previous_progress_ != ProgressIndicator::kProgressComplete) {
      // Progress is complete.
      EraseProgressIconAnimation();
      EnsureProgressRingAnimationOfType(ProgressRingAnimation::Type::kPulse);
    }
    previous_progress_ = progress;
  }

  // Invoked on update of the specified `animation`.
  void OnProgressRingAnimationUpdated(ProgressRingAnimation* animation) {
    if (animation->IsAnimating())
      return;

    // On completion, `animation` can be removed from the registry. This cannot
    // be done directly from `animation`'s subscription callback, so post a task
    // to delete `animation` as soon as possible.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::WeakPtr<DefaultProgressIndicatorAnimationRegistry>&
                   registry,
               ProgressRingAnimation* animation) {
              if (!registry)
                return;
              auto* key = registry->progress_indicator_.get();
              if (registry->GetProgressRingAnimationForKey(key) == animation)
                registry->SetProgressRingAnimationForKey(key, nullptr);
            },
            weak_ptr_factory_.GetWeakPtr(), animation));
  }

  // Ensures that a progress icon animation exists and is started.
  void EnsureProgressIconAnimation() {
    if (!GetProgressIconAnimationForKey(progress_indicator_)) {
      SetProgressIconAnimationForKey(progress_indicator_,
                                     std::make_unique<ProgressIconAnimation>())
          ->Start();
    }
  }

  // Ensures that a progress ring animation of the specified `type` exists and
  // is started.
  void EnsureProgressRingAnimationOfType(ProgressRingAnimation::Type type) {
    auto* ring_animation = GetProgressRingAnimationForKey(progress_indicator_);
    if (ring_animation && ring_animation->type() == type)
      return;

    auto animation = ProgressRingAnimation::CreateOfType(type);

    // NOTE: `animation` is owned by `this` so it is safe to use a raw pointer
    // and subscription-less callback.
    animation->AddUnsafeAnimationUpdatedCallback(
        base::BindRepeating(&DefaultProgressIndicatorAnimationRegistry::
                                OnProgressRingAnimationUpdated,
                            base::Unretained(this), animation.get()));

    SetProgressRingAnimationForKey(progress_indicator_, std::move(animation))
        ->Start();
  }

  // Erases any existing progress icon animation.
  void EraseProgressIconAnimation() {
    SetProgressIconAnimationForKey(progress_indicator_, nullptr);
  }

  // Erases any existing progress ring animation.
  void EraseProgressRingAnimation() {
    SetProgressRingAnimationForKey(progress_indicator_, nullptr);
  }

  // The progress indicator for which to manage animations and a subscription
  // to receive notification of progress change events.
  raw_ptr<ProgressIndicator, ExperimentalAsh> progress_indicator_ = nullptr;
  base::CallbackListSubscription progress_changed_subscription_;

  // Instantiate `previous_progress_` to completion to avoid starting a pulse
  // animation on first progress update.
  absl::optional<float> previous_progress_ =
      ProgressIndicator::kProgressComplete;

  base::WeakPtrFactory<DefaultProgressIndicatorAnimationRegistry>
      weak_ptr_factory_{this};
};

// DefaultProgressIndicator ----------------------------------------------------

// A default implementation of `ProgressIndicator` which paints indication of
// progress returned by the specified `progress_callback_`. NOTE: This instance
// comes pre-wired with an animation `registry_` that will manage progress
// animations as needed.
class DefaultProgressIndicator : public ProgressIndicator {
 public:
  DefaultProgressIndicator(
      std::unique_ptr<DefaultProgressIndicatorAnimationRegistry> registry,
      base::RepeatingCallback<absl::optional<float>()> progress_callback)
      : ProgressIndicator(/*registry=*/registry.get(),
                          /*animation_key=*/this),
        registry_(std::move(registry)),
        progress_callback_(std::move(progress_callback)) {
    registry_->SetProgressIndicator(this);
  }

  DefaultProgressIndicator(const DefaultProgressIndicator&) = delete;
  DefaultProgressIndicator& operator=(const DefaultProgressIndicator&) = delete;
  ~DefaultProgressIndicator() override = default;

 private:
  // ProgressIndicator:
  absl::optional<float> CalculateProgress() const override {
    return progress_callback_.Run();
  }

  std::unique_ptr<DefaultProgressIndicatorAnimationRegistry> registry_;
  base::RepeatingCallback<absl::optional<float>()> progress_callback_;
};

}  // namespace

// ProgressIndicator -----------------------------------------------------------

// static
constexpr char ProgressIndicator::kClassName[];
constexpr float ProgressIndicator::kProgressComplete;

ProgressIndicator::ProgressIndicator(
    ProgressIndicatorAnimationRegistry* animation_registry,
    const void* animation_key)
    : animation_registry_(animation_registry), animation_key_(animation_key) {
  if (!animation_registry_)
    return;

  // Register to be notified of changes to the icon animation associated with
  // this progress indicator's `animation_key_`. Note that it is safe to use a
  // raw pointer here since `this` owns the subscription.
  icon_animation_changed_subscription_ =
      animation_registry_->AddProgressIconAnimationChangedCallbackForKey(
          animation_key_,
          base::BindRepeating(
              &ProgressIndicator::OnProgressIconAnimationChanged,
              base::Unretained(this)));

  // If an `icon_animation` is already registered, perform additional
  // initialization.
  ProgressIconAnimation* icon_animation =
      animation_registry_->GetProgressIconAnimationForKey(animation_key_);
  if (icon_animation)
    OnProgressIconAnimationChanged(icon_animation);

  // Register to be notified of changes to the ring animation associated with
  // this progress indicator's `animation_key_`. Note that it is safe to use a
  // raw pointer here since `this` owns the subscription.
  ring_animation_changed_subscription_ =
      animation_registry_->AddProgressRingAnimationChangedCallbackForKey(
          animation_key_,
          base::BindRepeating(
              &ProgressIndicator::OnProgressRingAnimationChanged,
              base::Unretained(this)));

  // If `ring_animation` is already registered, perform additional
  // initialization.
  ProgressRingAnimation* ring_animation =
      animation_registry_->GetProgressRingAnimationForKey(animation_key_);
  if (ring_animation)
    OnProgressRingAnimationChanged(ring_animation);
}

ProgressIndicator::~ProgressIndicator() = default;

// static
std::unique_ptr<ProgressIndicator> ProgressIndicator::CreateDefaultInstance(
    base::RepeatingCallback<absl::optional<float>()> progress_callback) {
  return std::make_unique<DefaultProgressIndicator>(
      std::make_unique<DefaultProgressIndicatorAnimationRegistry>(),
      std::move(progress_callback));
}

base::CallbackListSubscription ProgressIndicator::AddProgressChangedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return progress_changed_callback_list_.Add(std::move(callback));
}

ui::Layer* ProgressIndicator::CreateLayer() {
  DCHECK(!layer());

  auto layer = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  layer->set_delegate(this);
  layer->SetFillsBoundsOpaquely(false);
  layer->SetName(kClassName);
  Reset(std::move(layer));

  return this->layer();
}

void ProgressIndicator::DestroyLayer() {
  if (layer())
    ReleaseLayer();
}

void ProgressIndicator::InvalidateLayer() {
  if (layer())
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
}

void ProgressIndicator::SetInnerIconVisible(bool visible) {
  if (inner_icon_visible_ == visible)
    return;

  inner_icon_visible_ = visible;

  // It's not necessary to invalidate the `layer()` if progress is complete
  // since the inner icon is only painted while progress is incomplete.
  if (progress_ != kProgressComplete)
    InvalidateLayer();
}

void ProgressIndicator::OnDeviceScaleFactorChanged(float old_scale,
                                                   float new_scale) {
  InvalidateLayer();
}

void ProgressIndicator::OnPaintLayer(const ui::PaintContext& context) {
  // Look up the associated `ring_animation` (if one exists).
  ProgressRingAnimation* ring_animation =
      animation_registry_
          ? animation_registry_->GetProgressRingAnimationForKey(animation_key_)
          : nullptr;

  // Unless `this` is animating, nothing will paint if `progress_` is complete.
  if (progress_ == kProgressComplete && !ring_animation)
    return;

  float start, end, outer_ring_opacity;
  if (ring_animation) {
    start = ring_animation->start_position();
    end = ring_animation->end_position();
    outer_ring_opacity = ring_animation->outer_ring_opacity();
  } else {
    start = 0.f;
    end = progress_.value();
    outer_ring_opacity = 1.f;
  }

  DCHECK_GE(start, 0.f);
  DCHECK_LE(start, 1.f);
  DCHECK_GE(end, 0.f);
  DCHECK_LE(end, 1.f);
  DCHECK_GE(outer_ring_opacity, 0.f);
  DCHECK_LE(outer_ring_opacity, 1.f);

  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();

  // The `canvas` should be flipped for RTL.
  gfx::ScopedCanvas scoped_canvas(recorder.canvas());
  scoped_canvas.FlipIfRTL(layer()->size().width());

  // Look up the associated `icon_animation` (if one exists).
  ProgressIconAnimation* icon_animation =
      animation_registry_
          ? animation_registry_->GetProgressIconAnimationForKey(animation_key_)
          : nullptr;

  if (icon_animation) {
    const float opacity = icon_animation->opacity();
    DCHECK_GE(opacity, 0.f);
    DCHECK_LE(opacity, 1.f);
    canvas->SaveLayerAlpha(SK_AlphaOPAQUE * opacity);
  }

  float outer_ring_stroke_width = GetOuterRingStrokeWidth(layer(), progress_);
  gfx::RectF bounds(gfx::SizeF(layer()->size()));
  bounds.Inset(gfx::InsetsF(outer_ring_stroke_width / 2.f));
  SkPath path(CreateRoundedRectPath(
      bounds, /*radius=*/std::min(bounds.width(), bounds.height()) / 2.f));

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStrokeCap(cc::PaintFlags::Cap::kDefault_Cap);
  flags.setStrokeWidth(outer_ring_stroke_width);
  flags.setStyle(cc::PaintFlags::Style::kStroke_Style);

  const SkColor color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  // Outer ring.
  flags.setColor(SkColorSetA(
      color,
      SK_AlphaOPAQUE * GetOuterRingOpacity(progress_) * outer_ring_opacity));
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

  // The inner ring and inner icon should be absent once progress completes.
  // This would occur if the progress ring is animating post completion.
  if (progress_ == kProgressComplete)
    return;

  float inner_ring_stroke_width = GetInnerRingStrokeWidth(layer());

  if (icon_animation) {
    inner_ring_stroke_width *=
        icon_animation->inner_ring_stroke_width_scale_factor();
  }

  const bool inner_ring_visible =
      !cc::MathUtil::IsWithinEpsilon(inner_ring_stroke_width, 0.f);

  // Inner ring.
  if (inner_ring_visible) {
    bounds.Inset(gfx::InsetsF(
        (outer_ring_stroke_width + inner_ring_stroke_width) / 2.f));
    path = CreateRoundedRectPath(
        bounds, /*radius=*/std::min(bounds.width(), bounds.height()) / 2.f);

    flags.setColor(color);
    flags.setStrokeWidth(inner_ring_stroke_width);
    canvas->DrawPath(path, flags);
  }

  // Inner icon.
  if (inner_icon_visible_) {
    float inner_icon_size = GetInnerIconSize(layer());
    gfx::RectF inner_icon_bounds(gfx::SizeF(layer()->size()));
    inner_icon_bounds.ClampToCenteredSize(
        gfx::SizeF(inner_icon_size, inner_icon_size));

    if (icon_animation) {
      inner_icon_bounds.Offset(
          /*horizontal=*/0.f,
          /*vertical=*/icon_animation->inner_icon_translate_y_scale_factor() *
              inner_icon_size);
    }

    gfx::Transform transform;
    transform.Translate(inner_icon_bounds.x(), inner_icon_bounds.y());
    canvas->Transform(transform);
    gfx::PaintVectorIcon(canvas, kHoldingSpaceDownloadIcon, inner_icon_size,
                         color);
  }
}

void ProgressIndicator::UpdateVisualState() {
  const auto previous_progress = progress_;

  // Cache `progress_`.
  progress_ = CalculateProgress();
  if (progress_.has_value()) {
    DCHECK_GE(progress_.value(), 0.f);
    DCHECK_LE(progress_.value(), 1.f);
  }

  // Notify `progress_` changes.
  if (progress_ != previous_progress)
    progress_changed_callback_list_.Notify();
}

void ProgressIndicator::OnProgressIconAnimationChanged(
    ProgressIconAnimation* animation) {
  // Trigger repaint of this progress indicator on `animation` updates. Note
  // that it is safe to use a raw pointer here since `this` owns the
  // subscription.
  if (animation) {
    icon_animation_updated_subscription_ =
        animation->AddAnimationUpdatedCallback(base::BindRepeating(
            &ProgressIndicator::InvalidateLayer, base::Unretained(this)));
  }
  InvalidateLayer();
}

void ProgressIndicator::OnProgressRingAnimationChanged(
    ProgressRingAnimation* animation) {
  // Trigger repaint of this progress indicator on `animation` updates. Note
  // that it is safe to use a raw pointer here since `this` owns the
  // subscription.
  if (animation) {
    ring_animation_updated_subscription_ =
        animation->AddAnimationUpdatedCallback(base::BindRepeating(
            &ProgressIndicator::InvalidateLayer, base::Unretained(this)));
  }
  InvalidateLayer();
}

}  // namespace ash

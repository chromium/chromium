// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include <math.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

const int kLayerAnimationsForMinimizeDurationMS = 200;

// Amount of time for the cross fade animation.
constexpr base::TimeDelta kCrossFadeDuration = base::Milliseconds(200);

constexpr base::TimeDelta kCrossFadeMaxDuration = base::Milliseconds(400);

// The default duration for an animation to float or unfloat a window.
static constexpr base::TimeDelta kFloatUnfloatDuration =
    base::Milliseconds(400);

// Durations for the brightness/grayscale fade animation, in milliseconds.
const int kBrightnessGrayscaleFadeDurationMs = 1000;

// Duration for fade in animation, in milliseconds.
const int kFadeInAnimationMs = 200;

// Brightness/grayscale values for hide/show window animations.
const float kWindowAnimation_HideBrightnessGrayscale = 1.f;
const float kWindowAnimation_ShowBrightnessGrayscale = 0.f;

const float kWindowAnimation_HideOpacity = 0.f;
const float kWindowAnimation_ShowOpacity = 1.f;

// Duration for gfx::Tween::ZERO animation of showing window.
constexpr base::TimeDelta kZeroAnimationMs = base::Milliseconds(300);

constexpr char kCrossFadeSmoothness[] =
    "Ash.Window.AnimationSmoothness.CrossFade";

base::TimeDelta GetCrossFadeDuration(aura::Window* window,
                                     const gfx::RectF& old_bounds,
                                     const gfx::Rect& new_bounds) {
  if (::wm::WindowAnimationsDisabled(window))
    return base::TimeDelta();

  int old_area = static_cast<int>(old_bounds.width() * old_bounds.height());
  int new_area = new_bounds.width() * new_bounds.height();
  int max_area = std::max(old_area, new_area);
  // Avoid divide by zero.
  if (max_area == 0)
    return kCrossFadeDuration;

  int delta_area = std::abs(old_area - new_area);
  // If the area didn't change, the animation is instantaneous.
  if (delta_area == 0)
    return kCrossFadeDuration;

  float factor = static_cast<float>(delta_area) / static_cast<float>(max_area);
  const auto kRange = kCrossFadeMaxDuration - kCrossFadeDuration;
  return kCrossFadeDuration + factor * kRange;
}

// Defines an observer that can be used to monitor the destruction of a given
// layer and dump without crashing when that happens. This is needed to
// investigate a crash that happens within `CrossFadeAnimationInternal()` due to
// using a layer after it has been destroyed.
// TODO(http://b/333095196): Remove this once the root cause of the crash is
// found and fixed.
class LayerDeletionDumper : public ui::LayerObserver {
 public:
  explicit LayerDeletionDumper(ui::Layer* layer) {
    observation_.Observe(layer);
  }
  LayerDeletionDumper(const LayerDeletionDumper&) = delete;
  LayerDeletionDumper& operator=(const LayerDeletionDumper&) = delete;
  ~LayerDeletionDumper() override = default;

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override {
    observation_.Reset();
    base::debug::DumpWithoutCrashing();
  }

 private:
  base::ScopedObservation<ui::Layer, ui::LayerObserver> observation_{this};
};

// Observer for a window cross-fade animation. If either the window closes or
// the layer's animation completes, it deletes the layer and removes itself as
// an observer.
class CrossFadeObserver : public aura::WindowObserver,
                          public ui::ImplicitAnimationObserver {
 public:
  // Observes |window| for destruction, but does not take ownership.
  // Takes ownership of |layer_owner| and its child layers.
  CrossFadeObserver(aura::Window* window,
                    std::unique_ptr<ui::LayerTreeOwner> layer_owner,
                    std::optional<std::string> histogram_name)
      : window_(window),
        layer_(window->layer()),
        layer_owner_(std::move(layer_owner)) {
    window_->AddObserver(this);

    smoothness_tracker_ =
        layer_->GetCompositor()->RequestNewThroughputTracker();
    smoothness_tracker_->Start(
        metrics_util::ForSmoothnessV3(base::BindRepeating(
            [](const std::optional<std::string>& histogram_name,
               int smoothness) {
              if (histogram_name) {
                DCHECK(!histogram_name->empty());
                base::UmaHistogramPercentage(*histogram_name, smoothness);
              } else {
                UMA_HISTOGRAM_PERCENTAGE(kCrossFadeSmoothness, smoothness);
              }
            },
            std::move(histogram_name))));
  }
  CrossFadeObserver(const CrossFadeObserver&) = delete;
  CrossFadeObserver& operator=(const CrossFadeObserver&) = delete;
  ~CrossFadeObserver() override {
    smoothness_tracker_->Stop();
    smoothness_tracker_.reset();

    // Stop the old animator to trigger aborts or ends on any observers it may
    // have.
    layer_owner_->root()->GetAnimator()->StopAnimating();

    window_->RemoveObserver(this);
    window_ = nullptr;
    layer_ = nullptr;
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override { StopAnimating(); }
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    StopAnimating();
  }
  void OnWindowLayerRecreated(aura::Window* window) override {
    // If the window layer is recreated. |layer_| will hold the LayerAnimator we
    // were originally observing. Layer recreation is usually done when doing
    // another window animation, so stop this current one and trigger
    // OnImplicitAnimationsCompleted to delete ourselves.
    layer_->GetAnimator()->StopAnimating();
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (auto* resize_shadow_controller =
            Shell::Get()->resize_shadow_controller()) {
      resize_shadow_controller->OnCrossFadeAnimationCompleted(window_);
    }
    delete this;
  }

 protected:
  void StopAnimating() {
    // Trigger OnImplicitAnimationsCompleted() to be called and deletes us. If
    // no animation is running then do the deletion ourselves.
    DCHECK(window_);
    window_->layer()->GetAnimator()->StopAnimating();
  }

  // The window and the associated layer this observer is watching. The window
  // layer may be recreated during the course of the animation so |layer_| will
  // be different |window_->layer()| after construction.
  raw_ptr<aura::Window> window_;
  raw_ptr<ui::Layer> layer_;

  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  std::optional<ui::ThroughputTracker> smoothness_tracker_;
};

// A version of CrossFadeObserver which updates its transform to match the
// visible bounds of the window it is cross-fading. It is expected users of this
// will not perform their own transform animation on the passed LayerTreeOwner.
class CrossFadeUpdateTransformObserver
    : public CrossFadeObserver,
      public ui::CompositorAnimationObserver {
 public:
  CrossFadeUpdateTransformObserver(
      aura::Window* window,
      std::unique_ptr<ui::LayerTreeOwner> layer_owner,
      std::optional<std::string> histogram_name)
      : CrossFadeObserver(window, std::move(layer_owner), histogram_name) {
    compositor_ = window->layer()->GetCompositor();
    compositor_->AddAnimationObserver(this);
  }
  CrossFadeUpdateTransformObserver(const CrossFadeUpdateTransformObserver&) =
      delete;
  CrossFadeUpdateTransformObserver& operator=(
      const CrossFadeUpdateTransformObserver&) = delete;
  ~CrossFadeUpdateTransformObserver() override {
    compositor_->RemoveAnimationObserver(this);
  }

  // CrossFadeObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {
    DCHECK(!layer_owner_->root()->GetAnimator()->IsAnimatingProperty(
        ui::LayerAnimationElement::TRANSFORM));
    CrossFadeObserver::OnLayerAnimationStarted(sequence);
  }

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override {
    // If these get shut down or destroyed we should delete ourselves.
    DCHECK(compositor_);
    DCHECK(window_);

    // Calculate the transform needed to place |layer_owner_| in the same bounds
    // plus transform as |window_|.
    gfx::RectF old_bounds(layer_owner_->root()->bounds());
    gfx::RectF new_bounds(window_->bounds());
    gfx::Transform new_transform = window_->transform();
    DCHECK(new_transform.IsScaleOrTranslation());

    // Apply the transform on the bounds to get the location of |window_|.
    // Transforms are calculated in a way where scale does not affect position,
    // so use the same logic here.
    gfx::RectF effective_bounds =
        new_transform.MapRect(gfx::RectF(new_bounds.size()));
    effective_bounds.Offset(new_bounds.OffsetFromOrigin());

    const gfx::Transform old_transform =
        gfx::TransformBetweenRects(old_bounds, effective_bounds);
    layer_owner_->root()->SetTransform(old_transform);
  }

  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor_, compositor);
    StopAnimating();
  }

 private:
  raw_ptr<ui::Compositor> compositor_ = nullptr;
};

// Internal implementation of a cross fade animation. If
// |animate_old_layer_transform| is true, both new and old layers will animate
// their opacities and transforms. Otherwise, the old layer will on animate its
// opacity; its transforms will be updated via an observer.
void CrossFadeAnimationInternal(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner,
    bool animate_old_layer_transform,
    std::optional<base::TimeDelta> duration,
    std::optional<gfx::Tween::Type> tween_type,
    std::optional<std::string> histogram_name) {
  ui::Layer* old_layer = old_layer_owner->root();

  // The window's layer can get recreated within the stack of this function due
  // to various reasons. We should never cache the layer as local, and always
  // get the layer from the window directly. See http://b/333095196 and
  // http://b/40059305. For example in http://b/40059305, if Overview exit
  //  animation is in the sequence, stopping animation may trigger
  // `OverviewController::OnEndingAnimationComplete` which may finally start the
  // frame animation. 'FrameHeader::FrameAnimatorView::StartAnimation' recreates
  // the window's layer.
  auto new_layer = [window]() -> ui::Layer* { return window->layer(); };

  DCHECK(old_layer);
  const gfx::Rect old_bounds(old_layer->bounds());

  gfx::Transform old_transform(old_layer->transform());
  gfx::Transform old_transform_in_root;
  old_transform_in_root.Translate(old_bounds.x(), old_bounds.y());
  old_transform_in_root.PreConcat(old_transform);
  old_transform_in_root.Translate(-old_bounds.x(), -old_bounds.y());
  gfx::RectF old_transformed_bounds =
      old_transform_in_root.MapRect(gfx::RectF(old_bounds));
  const gfx::Rect new_bounds(window->bounds());
  const bool old_on_top = (old_bounds.width() > new_bounds.width());

  SCOPED_CRASH_KEY_BOOL("333095196", "animate_old_layer_transform",
                        animate_old_layer_transform);
  SCOPED_CRASH_KEY_BOOL("333095196", "old_on_top", old_on_top);
  SCOPED_CRASH_KEY_STRING256("333095196", "window_title",
                             base::UTF16ToUTF8(window->GetTitle()));
  SCOPED_CRASH_KEY_STRING256("333095196", "new_layer_begin",
                             base::StringPrintf("%p", new_layer()));

  // Ensure the higher-resolution layer is on top.
  if (old_on_top)
    old_layer->parent()->StackBelow(new_layer(), old_layer);
  else
    old_layer->parent()->StackAbove(new_layer(), old_layer);

  // Shorten the animation if there's not much visual movement.
  const base::TimeDelta animation_duration = duration.value_or(
      GetCrossFadeDuration(window, old_transformed_bounds, new_bounds));
  const gfx::Tween::Type animation_tween_type =
      tween_type.value_or(gfx::Tween::EASE_OUT);

  // Scale up the old layer while translating to new position.
  {
    old_layer->GetAnimator()->StopAnimating();
    old_layer->SetTransform(old_transform);
    ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());
    settings.SetTransitionDuration(animation_duration);
    settings.SetTweenType(animation_tween_type);
    settings.DeferPaint();

    if (old_on_top) {
      // Only caching render surface when there is an opacity animation and
      // multiple layers.
      if (!old_layer->children().empty())
        settings.CacheRenderSurface();
      // The old layer is on top, and should fade out. The new layer below will
      // stay opaque to block the desktop.
      old_layer->SetOpacity(kWindowAnimation_HideOpacity);
    } else if (!animate_old_layer_transform) {
      // If |animate_old_layer_transform| and |old_on_top| are both false, then
      // the old layer will have no animations and the observer will delete
      // itself (and the old layer) right away. To make sure the old layer stays
      // behind the new layer with opacity 1.f for the duration of the
      // animation, change the tween to zero.
      settings.SetTweenType(gfx::Tween::ZERO);
      old_layer->SetOpacity(kWindowAnimation_HideOpacity);
    }

    if (animate_old_layer_transform) {
      gfx::Transform out_transform;
      float scale_x =
          new_bounds.width() / static_cast<float>(old_bounds.width());
      float scale_y =
          new_bounds.height() / static_cast<float>(old_bounds.height());
      out_transform.Translate(new_bounds.x() - old_bounds.x(),
                              new_bounds.y() - old_bounds.y());
      out_transform.Scale(scale_x, scale_y);
      old_layer->SetTransform(out_transform);
    }
    // In tests |old_layer| is deleted here, as animations have zero duration.
    old_layer = nullptr;
  }

  SCOPED_CRASH_KEY_STRING256("333095196", "new_layer_mid",
                             base::StringPrintf("%p", new_layer()));

  // Create an observer that would dump without crashing if `new_layer()` gets
  // destroyed within the remaining scope of this function. This is needed to
  // investigate the root cause of http://b/333095196.
  // TODO(http://b/333095196): Remove this code and all crash keys once the
  // issue is resolved.
  LayerDeletionDumper deletion_dumper(new_layer());

  // Set the new layer's current transform, such that the user sees a scaled
  // version of the window with the original bounds at the original position.
  gfx::Transform in_transform;
  const float scale_x =
      old_transformed_bounds.width() / static_cast<float>(new_bounds.width());
  const float scale_y =
      old_transformed_bounds.height() / static_cast<float>(new_bounds.height());
  in_transform.Translate(old_transformed_bounds.x() - new_bounds.x(),
                         old_transformed_bounds.y() - new_bounds.y());
  in_transform.Scale(scale_x, scale_y);
  new_layer()->SetTransform(in_transform);
  if (!old_on_top) {
    // The new layer is on top and should fade in.  The old layer below will
    // stay opaque and block the desktop.
    new_layer()->SetOpacity(kWindowAnimation_HideOpacity);
  }
  {
    // Animation observer owns the old layer and deletes itself. It should be
    // attached to the new layer so that if the new layer animation gets
    // aborted, we can delete the old layer.
    CrossFadeObserver* observer =
        animate_old_layer_transform
            ? new CrossFadeObserver(window, std::move(old_layer_owner),
                                    histogram_name)
            : new CrossFadeUpdateTransformObserver(
                  window, std::move(old_layer_owner), histogram_name);

    // Animate the new layer to the identity transform, so the window goes to
    // its newly set bounds.
    ui::ScopedLayerAnimationSettings settings(new_layer()->GetAnimator());
    settings.AddObserver(observer);
    settings.SetTransitionDuration(animation_duration);
    settings.SetTweenType(animation_tween_type);
    settings.DeferPaint();
    if (!old_on_top) {
      // Only caching render surface when there is an opacity animation and
      // multiple layers.
      if (!new_layer()->children().empty()) {
        settings.CacheRenderSurface();
      }
      // New layer is on top, fade it in.
      new_layer()->SetOpacity(kWindowAnimation_ShowOpacity);
    }
    SCOPED_CRASH_KEY_NUMBER("333095196", "animation_duration_ms",
                            animation_duration.InMillisecondsF());
    SCOPED_CRASH_KEY_BOOL("333095196", "is_destroying",
                          window->is_destroying());
    SCOPED_CRASH_KEY_STRING256("333095196", "new_layer_end",
                               base::StringPrintf("%p", new_layer()));
    new_layer()->SetTransform(gfx::Transform());
  }
}

}  // namespace

void SetTransformForScaleAnimation(ui::Layer* layer,
                                   LayerScaleAnimationDirection type) {
  // Scales for windows above and below the current workspace.
  constexpr float kLayerScaleAboveSize = 1.1f;
  constexpr float kLayerScaleBelowSize = .9f;

  const float scale = type == LAYER_SCALE_ANIMATION_ABOVE
                          ? kLayerScaleAboveSize
                          : kLayerScaleBelowSize;
  gfx::Transform transform;
  transform.Translate(-layer->bounds().width() * (scale - 1.0f) / 2,
                      -layer->bounds().height() * (scale - 1.0f) / 2);
  transform.Scale(scale, scale);
  layer->SetTransform(transform);
}

void AddLayerAnimationsForMinimize(aura::Window* window, bool show) {
  // Recalculate the transform at restore time since the launcher item may have
  // moved while the window was minimized.
  gfx::Rect bounds = window->bounds();
  gfx::Rect target_bounds = GetMinimizeAnimationTargetBoundsInScreen(window);
  ::wm::ConvertRectFromScreen(window->parent(), &target_bounds);

  float scale_x = static_cast<float>(target_bounds.width()) / bounds.width();
  float scale_y = static_cast<float>(target_bounds.height()) / bounds.height();

  std::unique_ptr<ui::InterpolatedTransform> scale =
      std::make_unique<ui::InterpolatedScale>(
          gfx::Point3F(1, 1, 1), gfx::Point3F(scale_x, scale_y, 1));

  std::unique_ptr<ui::InterpolatedTransform> translation =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(), gfx::PointF(target_bounds.x() - bounds.x(),
                                     target_bounds.y() - bounds.y()));

  scale->SetChild(std::move(translation));
  scale->SetReversed(show);

  base::TimeDelta duration =
      window->layer()->GetAnimator()->GetTransitionDuration();

  std::unique_ptr<ui::LayerAnimationElement> transition =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(scale), duration);

  transition->set_tween_type(show ? gfx::Tween::EASE_IN
                                  : gfx::Tween::EASE_IN_OUT);

  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(std::move(transition)));

  // When hiding a window, turn off blending until the animation is 3 / 4 done
  // to save bandwidth and reduce jank.
  if (!show) {
    window->layer()->GetAnimator()->SchedulePauseForProperties(
        (duration * 3) / 4, ui::LayerAnimationElement::OPACITY);
  }

  // Fade in and out quickly when the window is small to reduce jank.
  float opacity = show ? 1.0f : 0.0f;
  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateOpacityElement(opacity,
                                                          duration / 4)));

  // Reset the transform to identity when the minimize animation is completed.
  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateTransformElement(
              gfx::Transform(), base::TimeDelta())));
}

void AnimateShowWindow_Minimize(aura::Window* window) {
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  ui::AnimationThroughputReporter reporter(
      settings.GetAnimator(),
      metrics_util::ForSmoothnessV3(
          base::BindRepeating(static_cast<void (*)(const char*, int)>(
                                  &base::UmaHistogramPercentage),
                              "Ash.Window.AnimationSmoothness.Unminimize")));
  base::TimeDelta duration =
      base::Milliseconds(kLayerAnimationsForMinimizeDurationMS);
  settings.SetTransitionDuration(duration);
  AddLayerAnimationsForMinimize(window, true);

  // Now that the window has been restored, we need to clear its animation style
  // to default so that normal animation applies.
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
}

void AnimateHideWindow_Minimize(aura::Window* window) {
  // Property sets within this scope will be implicitly animated.
  ::wm::ScopedHidingAnimationSettings hiding_settings(window);
  // Report animation smoothness for animations created within this scope.
  ui::AnimationThroughputReporter reporter(
      hiding_settings.layer_animation_settings()->GetAnimator(),
      metrics_util::ForSmoothnessV3(
          base::BindRepeating(static_cast<void (*)(const char*, int)>(
                                  &base::UmaHistogramPercentage),
                              "Ash.Window.AnimationSmoothness.Minimize")));

  base::TimeDelta duration =
      base::Milliseconds(kLayerAnimationsForMinimizeDurationMS);

  hiding_settings.layer_animation_settings()->SetTransitionDuration(duration);
  window->layer()->SetVisible(false);

  AddLayerAnimationsForMinimize(window, false);
}

void AnimateShowHideWindowCommon_BrightnessGrayscale(aura::Window* window,
                                                     bool show) {
  float start_value, end_value;
  if (show) {
    start_value = kWindowAnimation_HideBrightnessGrayscale;
    end_value = kWindowAnimation_ShowBrightnessGrayscale;
  } else {
    start_value = kWindowAnimation_ShowBrightnessGrayscale;
    end_value = kWindowAnimation_HideBrightnessGrayscale;
  }

  window->layer()->SetLayerBrightness(start_value);
  window->layer()->SetLayerGrayscale(start_value);
  if (show) {
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
    window->layer()->SetVisible(true);
  }

  base::TimeDelta duration =
      base::Milliseconds(kBrightnessGrayscaleFadeDurationMs);

  if (show) {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    window->layer()->GetAnimator()->ScheduleTogether(
        CreateBrightnessGrayscaleAnimationSequence(end_value, duration));
  } else {
    ::wm::ScopedHidingAnimationSettings hiding_settings(window);
    window->layer()->GetAnimator()->ScheduleTogether(
        CreateBrightnessGrayscaleAnimationSequence(end_value, duration));
    window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
    window->layer()->SetVisible(false);
  }
}

void AnimateShowWindow_BrightnessGrayscale(aura::Window* window) {
  AnimateShowHideWindowCommon_BrightnessGrayscale(window, true);
}

// TODO(edcourtney): Consolidate with AnimateShowWindow_Fade in ui/wm/core.
void AnimateShowWindow_FadeIn(aura::Window* window) {
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetTransitionDuration(base::Milliseconds(kFadeInAnimationMs));
  window->layer()->SetVisible(true);
  window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
}

void AnimateHideWindow_BrightnessGrayscale(aura::Window* window) {
  AnimateShowHideWindowCommon_BrightnessGrayscale(window, false);
}

void AnimateHideWindow_SlideOut(aura::Window* window) {
  base::TimeDelta duration =
      base::Milliseconds(PipPositioner::kPipDismissTimeMs);

  ::wm::ScopedHidingAnimationSettings settings(window);
  settings.layer_animation_settings()->SetTransitionDuration(duration);
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetVisible(false);

  gfx::Rect bounds = window->GetBoundsInScreen();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  gfx::Rect dismissed_bounds =
      PipPositioner::GetDismissedPosition(display, bounds);
  ::wm::ConvertRectFromScreen(window->parent(), &dismissed_bounds);
  window->layer()->SetBounds(dismissed_bounds);
}

void AnimateShowWindow_StepEnd(aura::Window* window) {
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetTransitionDuration(kZeroAnimationMs);
  settings.SetTweenType(gfx::Tween::ZERO);
  window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
}

void AnimateHideWindow_StepEnd(aura::Window* window) {
  ::wm::ScopedHidingAnimationSettings settings(window);
  settings.layer_animation_settings()->SetTransitionDuration(kZeroAnimationMs);
  settings.layer_animation_settings()->SetTweenType(gfx::Tween::ZERO);
  window->layer()->SetVisible(false);
}

bool AnimateShowWindow(aura::Window* window) {
  if (!::wm::HasWindowVisibilityAnimationTransition(window,
                                                    ::wm::ANIMATE_SHOW)) {
    return false;
  }

  switch (::wm::GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE:
      AnimateShowWindow_Minimize(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE:
      AnimateShowWindow_BrightnessGrayscale(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE_IN_SLIDE_OUT:
      AnimateShowWindow_FadeIn(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_STEP_END:
      AnimateShowWindow_StepEnd(window);
      return true;
    default:
      NOTREACHED();
  }
}

bool AnimateHideWindow(aura::Window* window) {
  if (!::wm::HasWindowVisibilityAnimationTransition(window,
                                                    ::wm::ANIMATE_HIDE)) {
    return false;
  }

  switch (::wm::GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE:
      AnimateHideWindow_Minimize(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE:
      AnimateHideWindow_BrightnessGrayscale(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE_IN_SLIDE_OUT:
      AnimateHideWindow_SlideOut(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_STEP_END:
      AnimateHideWindow_StepEnd(window);
      return true;
    default:
      NOTREACHED();
  }
}

void CrossFadeAnimation(aura::Window* window,
                        std::unique_ptr<ui::LayerTreeOwner> old_layer_owner) {
  CrossFadeAnimationInternal(
      window, std::move(old_layer_owner), /*animate_old_layer_transform=*/true,
      /*duration=*/std::nullopt, /*tween_type=*/std::nullopt,
      /*histogram_name=*/std::nullopt);
}

void CrossFadeAnimationForFloatUnfloat(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner,
    bool to_float) {
  CrossFadeAnimationInternal(window, std::move(old_layer_owner),
                             /*animate_old_layer_transform=*/true,
                             kFloatUnfloatDuration,
                             to_float ? gfx::Tween::Type::ACCEL_30_DECEL_20_85
                                      : gfx::Tween::Type::FAST_OUT_SLOW_IN_3,
                             /*histogram_name=*/std::nullopt);
}

void CrossFadeAnimationAnimateNewLayerOnly(aura::Window* window,
                                           const gfx::Rect& target_bounds,
                                           base::TimeDelta duration,
                                           gfx::Tween::Type tween_type,
                                           const std::string& histogram_name) {
  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      ::wm::RecreateLayers(window);
  window->SetBounds(target_bounds);
  CrossFadeAnimationInternal(window, std::move(old_layer_owner),
                             /*animate_old_layer=*/false, duration, tween_type,
                             histogram_name);
}

bool AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (::wm::WindowAnimationsDisabled(window))
    return false;
  // Attempt to run CoreWm supplied animation types.
  if (::wm::AnimateOnChildWindowVisibilityChanged(window, visible))
    return true;
  // Otherwise try to run an Ash-specific animation.
  if (visible)
    return AnimateShowWindow(window);
  // Don't start hiding the window again if it's already being hidden.
  return window->layer()->GetTargetOpacity() != 0.0f &&
         AnimateHideWindow(window);
}

std::vector<ui::LayerAnimationSequence*>
CreateBrightnessGrayscaleAnimationSequence(float target_value,
                                           base::TimeDelta duration) {
  gfx::Tween::Type animation_type = gfx::Tween::EASE_OUT;
  std::unique_ptr<ui::LayerAnimationSequence> brightness_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  std::unique_ptr<ui::LayerAnimationSequence> grayscale_sequence =
      std::make_unique<ui::LayerAnimationSequence>();

  std::unique_ptr<ui::LayerAnimationElement> brightness_element =
      ui::LayerAnimationElement::CreateBrightnessElement(target_value,
                                                         duration);
  brightness_element->set_tween_type(animation_type);
  brightness_sequence->AddElement(std::move(brightness_element));

  std::unique_ptr<ui::LayerAnimationElement> grayscale_element =
      ui::LayerAnimationElement::CreateGrayscaleElement(target_value, duration);
  grayscale_element->set_tween_type(animation_type);
  grayscale_sequence->AddElement(std::move(grayscale_element));

  std::vector<ui::LayerAnimationSequence*> animations;
  animations.push_back(brightness_sequence.release());
  animations.push_back(grayscale_sequence.release());

  return animations;
}

gfx::Rect GetMinimizeAnimationTargetBoundsInScreen(aura::Window* window) {
  Shelf* shelf = Shelf::ForWindow(window);
  gfx::Rect item_rect = shelf->GetScreenBoundsOfItemIconForWindow(window);

  // The launcher item is visible and has an icon.
  if (!item_rect.IsEmpty())
    return item_rect;

  // If both the icon width and height are 0, then there is no icon in the
  // launcher for |window|. If the launcher is auto hidden, one of the height or
  // width will be 0 but the position in the launcher and the major dimension
  // are still reported correctly and the window can be animated to the launcher
  // item's light bar.
  if (item_rect.width() != 0 || item_rect.height() != 0) {
    if (shelf->GetVisibilityState() == SHELF_AUTO_HIDE) {
      gfx::Rect shelf_bounds = shelf->GetWindow()->GetBoundsInScreen();
      if (shelf->alignment() == ShelfAlignment::kLeft)
        item_rect.set_x(shelf_bounds.right());
      else if (shelf->alignment() == ShelfAlignment::kRight)
        item_rect.set_x(shelf_bounds.x());
      else
        item_rect.set_y(shelf_bounds.y());
      return item_rect;
    }
  }

  // Coming here, there is no visible icon of that shelf item and we zoom back
  // to the location of the application launcher (which is fixed as first item
  // of the shelf).
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  int ltr_adjusted_x = base::i18n::IsRTL() ? work_area.right() : work_area.x();
  switch (shelf->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return gfx::Rect(ltr_adjusted_x, work_area.bottom(), 0, 0);
    case ShelfAlignment::kLeft:
      return gfx::Rect(work_area.x(), work_area.y(), 0, 0);
    case ShelfAlignment::kRight:
      return gfx::Rect(work_area.right(), work_area.y(), 0, 0);
  }
  NOTREACHED();
}

void BounceWindow(aura::Window* window) {
  wm::AnimateWindow(window, wm::WINDOW_ANIMATION_TYPE_BOUNCE);
}

}  // namespace ash

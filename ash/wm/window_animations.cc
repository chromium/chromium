// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include <math.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

const int kLayerAnimationsForMinimizeDurationMS = 200;

constexpr base::TimeDelta kCrossFadeMaxDuration =
    base::TimeDelta::FromMilliseconds(400);

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
constexpr base::TimeDelta kZeroAnimationMs =
    base::TimeDelta::FromMilliseconds(300);

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

class CrossFadeMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  CrossFadeMetricsReporter() = default;
  ~CrossFadeMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Ash.Window.AnimationSmoothness.CrossFade", value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossFadeMetricsReporter);
};

base::LazyInstance<CrossFadeMetricsReporter>::Leaky g_reporter_cross_fade =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

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
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kLayerAnimationsForMinimizeDurationMS);
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
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kLayerAnimationsForMinimizeDurationMS);
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
      base::TimeDelta::FromMilliseconds(kBrightnessGrayscaleFadeDurationMs);

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
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFadeInAnimationMs));
  window->layer()->SetVisible(true);
  window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
}

void AnimateHideWindow_BrightnessGrayscale(aura::Window* window) {
  AnimateShowHideWindowCommon_BrightnessGrayscale(window, false);
}

void AnimateHideWindow_SlideOut(aura::Window* window) {
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(PipPositioner::kPipDismissTimeMs);

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
      return false;
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
      return false;
  }
}

// Observer for a window cross-fade animation. If either the window closes or
// the layer's animation completes, it deletes the layer and removes itself as
// an observer.
class CrossFadeObserver : public aura::WindowObserver,
                          public ui::ImplicitAnimationObserver {
 public:
  // Observes |window| for destruction, but does not take ownership.
  // Takes ownership of |layer| and its child layers.
  CrossFadeObserver(aura::Window* window,
                    std::unique_ptr<ui::LayerTreeOwner> layer_owner)
      : window_(window), layer_owner_(std::move(layer_owner)) {
    window_->AddObserver(this);
  }
  ~CrossFadeObserver() override {
    window_->RemoveObserver(this);
    window_ = NULL;
  }

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override {
    // Triggers OnImplicitAnimationsCompleted() to be called and deletes us.
    layer_owner_->root()->GetAnimator()->StopAnimating();
  }
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    layer_owner_->root()->GetAnimator()->StopAnimating();
  }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  aura::Window* window_;  // not owned
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  DISALLOW_COPY_AND_ASSIGN(CrossFadeObserver);
};

base::TimeDelta CrossFadeAnimation(
    aura::Window* window,
    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner) {
  ui::Layer* old_layer = old_layer_owner->root();
  ui::Layer* new_layer = window->layer();

  DCHECK(old_layer);
  const gfx::Rect old_bounds(old_layer_owner->root()->bounds());

  gfx::RectF old_transformed_bounds(old_bounds);
  gfx::Transform old_transform(old_layer_owner->root()->transform());
  gfx::Transform old_transform_in_root;
  old_transform_in_root.Translate(old_bounds.x(), old_bounds.y());
  old_transform_in_root.PreconcatTransform(old_transform);
  old_transform_in_root.Translate(-old_bounds.x(), -old_bounds.y());
  old_transform_in_root.TransformRect(&old_transformed_bounds);
  const gfx::Rect new_bounds(window->bounds());
  const bool old_on_top = (old_bounds.width() > new_bounds.width());

  // Ensure the higher-resolution layer is on top.
  if (old_on_top)
    old_layer->parent()->StackBelow(new_layer, old_layer);
  else
    old_layer->parent()->StackAbove(new_layer, old_layer);

  // Shorten the animation if there's not much visual movement.
  const base::TimeDelta duration =
      GetCrossFadeDuration(window, old_transformed_bounds, new_bounds);

  // Scale up the old layer while translating to new position.
  {
    ui::Layer* old_layer = old_layer_owner->root();
    old_layer->GetAnimator()->StopAnimating();
    old_layer->SetTransform(old_transform);
    ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());
    // Animation observer owns the old layer and deletes itself.
    settings.AddObserver(
        new CrossFadeObserver(window, std::move(old_layer_owner)));
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    // Only add reporter to |old_layer|.
    settings.SetAnimationMetricsReporter(g_reporter_cross_fade.Pointer());
    settings.DeferPaint();
    if (old_on_top) {
      // Only caching render surface when there is an opacity animation and
      // multiple layers.
      if (!old_layer->children().empty())
        settings.CacheRenderSurface();
      // The old layer is on top, and should fade out.  The new layer below will
      // stay opaque to block the desktop.
      old_layer->SetOpacity(kWindowAnimation_HideOpacity);
    }
    gfx::Transform out_transform;
    float scale_x = static_cast<float>(new_bounds.width()) /
                    static_cast<float>(old_bounds.width());
    float scale_y = static_cast<float>(new_bounds.height()) /
                    static_cast<float>(old_bounds.height());
    out_transform.Translate(new_bounds.x() - old_bounds.x(),
                            new_bounds.y() - old_bounds.y());
    out_transform.Scale(scale_x, scale_y);
    old_layer->SetTransform(out_transform);
    // In tests |old_layer| is deleted here, as animations have zero duration.
    old_layer = NULL;
  }

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
  new_layer->SetTransform(in_transform);
  if (!old_on_top) {
    // The new layer is on top and should fade in.  The old layer below will
    // stay opaque and block the desktop.
    new_layer->SetOpacity(kWindowAnimation_HideOpacity);
  }
  {
    // Animate the new layer to the identity transform, so the window goes to
    // its newly set bounds.
    ui::ScopedLayerAnimationSettings settings(new_layer->GetAnimator());
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    settings.DeferPaint();
    if (!old_on_top) {
      // Only caching render surface when there is an opacity animation and
      // multiple layers.
      if (!new_layer->children().empty())
        settings.CacheRenderSurface();
      // New layer is on top, fade it in.
      new_layer->SetOpacity(kWindowAnimation_ShowOpacity);
    }
    new_layer->SetTransform(gfx::Transform());
  }
  return duration;
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
      if (shelf->alignment() == SHELF_ALIGNMENT_LEFT)
        item_rect.set_x(shelf_bounds.right());
      else if (shelf->alignment() == SHELF_ALIGNMENT_RIGHT)
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
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return gfx::Rect(ltr_adjusted_x, work_area.bottom(), 0, 0);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Rect(work_area.x(), work_area.y(), 0, 0);
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Rect(work_area.right(), work_area.y(), 0, 0);
  }
  NOTREACHED();
  return gfx::Rect();
}

}  // namespace ash

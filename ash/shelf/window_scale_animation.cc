// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/window_scale_animation.h"

#include <optional>
#include <vector>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/wm/core/scoped_animation_disabler.h"

namespace ash {

namespace {

// The time to do window transform to scale up to its original position or
// scale down to homescreen animation.
constexpr base::TimeDelta kWindowScaleUpOrDownTime = base::Milliseconds(350);

// The delay to do window opacity fade out when scaling down the dragged window.
constexpr base::TimeDelta kWindowFadeOutDelay = base::Milliseconds(100);

// The window scale down factor if we head to home screen after drag ends.
constexpr float kWindowScaleDownFactor = 0.001f;

// The fast animation time to do window transform for transient child window.
// This will only be used in tests.
constexpr base::TimeDelta kFastAnimationTime = base::Milliseconds(100);

// This will only be updated to true in tests via
// |EnableScopedFastAnimationForTransientChildForTest()|.
bool g_should_use_fast_animation_for_transient_child = false;

// Returns the transform that should be applied to |window| if we should head to
// shelf after dragging.
gfx::Transform GetWindowTransformToShelf(aura::Window* window) {
  // The origin of bounds returned by GetBoundsInScreen() is transformed using
  // the window's transform. The transform that should be applied to the
  // window is calculated relative to the window bounds with no transforms
  // applied, and thus need the un-transformed window origin.
  const gfx::RectF window_bounds(window->GetBoundsInScreen());
  gfx::PointF origin = window_bounds.origin();
  gfx::PointF origin_without_transform =
      window->transform().InverseMapPoint(origin).value_or(origin);

  gfx::Transform transform;
  Shelf* shelf = Shelf::ForWindow(window);

  const gfx::Rect shelf_item_bounds =
      shelf->GetScreenBoundsOfItemIconForWindow(window);

  if (!shelf_item_bounds.IsEmpty()) {
    const gfx::RectF shelf_item_bounds_f(shelf_item_bounds);
    const gfx::PointF shelf_item_center = shelf_item_bounds_f.CenterPoint();
    transform.Translate(shelf_item_center.x() - origin_without_transform.x(),
                        shelf_item_center.y() - origin_without_transform.y());
    transform.Scale(shelf_item_bounds_f.width() / window_bounds.width(),
                    shelf_item_bounds_f.height() / window_bounds.height());
  } else {
    const gfx::PointF shelf_center_point =
        gfx::RectF(shelf->GetIdealBounds()).CenterPoint();
    transform.Translate(shelf_center_point.x() - origin_without_transform.x(),
                        shelf_center_point.y() - origin_without_transform.y());
    transform.Scale(kWindowScaleDownFactor, kWindowScaleDownFactor);
  }
  return transform;
}

base::TimeDelta GetWindowAnimationTime(aura::Window* window) {
  if (g_should_use_fast_animation_for_transient_child &&
      window != wm::GetTransientRoot(window)) {
    return kFastAnimationTime;
  }
  return kWindowScaleUpOrDownTime;
}

}  // namespace

// -----------------------------------------------------------------------------
// WindowScaleAnimation::AnimationObserver:

// It's owned by |WindowScaleAnimation| and will be destroyed when its
// |window_| animation is completed or its |window_| is being destroyed.
class WindowScaleAnimation::AnimationObserver
    : public ui::ImplicitAnimationObserver,
      public aura::WindowObserver {
 public:
  AnimationObserver(aura::Window* window,
                    WindowScaleAnimation* window_scale_animation)
      : window_(window), window_scale_animation_(window_scale_animation) {
    window_observation_.Observe(window_.get());
  }

  AnimationObserver(const AnimationObserver&) = delete;
  AnimationObserver& operator=(const AnimationObserver&) = delete;
  ~AnimationObserver() override {
    // Explicitly stopping observing will prevent
    // `OnImplicitAnimationsCompleted()` from being called.
    StopObservingImplicitAnimations();
  }

  aura::Window* window() { return window_; }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    window_scale_animation_->DestroyWindowAnimationObserver(this);
    // |this| is destroyed after the above line.
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window_scale_animation_->DestroyWindowAnimationObserver(this);
    // |this| is destroyed after the above line.
  }

 private:
  // Pointers to the window and the parent scale animation. Guaranteed to
  // outlive `this`.
  const raw_ptr<aura::Window> window_;

  const raw_ptr<WindowScaleAnimation> window_scale_animation_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

WindowScaleAnimation::WindowScaleAnimation(WindowScaleType scale_type,
                                           base::OnceClosure opt_callback)
    : opt_callback_(std::move(opt_callback)), scale_type_(scale_type) {}

WindowScaleAnimation::~WindowScaleAnimation() {
  if (!opt_callback_.is_null())
    std::move(opt_callback_).Run();
}

void WindowScaleAnimation::Start(aura::Window* window) {
  // In the destructor of `ScopedLayerAnimationSettings`, it will activate all
  // of its observers. What we want is to activate the observer for each
  // transient child window after the for loop is done, otherwise `this` can be
  // early released via `WindowScaleAnimation::DestroyWindowAnimationObserver`.
  // Hence creating this vector outside of the for loop.
  std::vector<std::unique_ptr<ui::ScopedLayerAnimationSettings>> all_settings;
  for (auto* transient_window : GetTransientTreeIterator(window)) {
    window_animation_observers_.push_back(
        std::make_unique<AnimationObserver>(transient_window, this));
    WindowBackdrop::Get(transient_window)->DisableBackdrop();
    all_settings.push_back(std::make_unique<ui::ScopedLayerAnimationSettings>(
        transient_window->layer()->GetAnimator()));
    auto* settings = all_settings.back().get();
    settings->SetTransitionDuration(GetWindowAnimationTime(transient_window));
    settings->SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    settings->AddObserver(window_animation_observers_.back().get());
    if (scale_type_ == WindowScaleType::kScaleDownToShelf) {
      transient_window->layer()->GetAnimator()->SchedulePauseForProperties(
          kWindowFadeOutDelay, ui::LayerAnimationElement::OPACITY);
      transient_window->layer()->SetTransform(
          GetWindowTransformToShelf(transient_window));
      transient_window->layer()->SetOpacity(0.f);
    } else {
      transient_window->layer()->SetTransform(gfx::Transform());
    }
  }
}

// static
base::AutoReset<bool>
WindowScaleAnimation::EnableScopedFastAnimationForTransientChildForTest() {
  return base::AutoReset<bool>(&g_should_use_fast_animation_for_transient_child,
                               true);
}

void WindowScaleAnimation::DestroyWindowAnimationObserver(
    WindowScaleAnimation::AnimationObserver* animation_observer) {
  // `animation_observer` will get deleted on the next line.
  auto* window = animation_observer->window();

  std::erase_if(window_animation_observers_,
                base::MatchesUniquePtr(animation_observer));

  if (window_animation_observers_.empty()) {
    // Do the scale transform for the entire transient tree.
    OnScaleWindowsOnAnimationsCompleted(window);
    // self-destructed when all windows' transform animation is done.
    delete this;
  }
}

void WindowScaleAnimation::OnScaleWindowsOnAnimationsCompleted(
    aura::Window* window) {
  // Scale-down or scale-up window(s) with the windows' descending order
  // in the transient tree. We need to use this fixed order to ensure the
  // transient child window will be visible after returning back from home
  // screen to the window. If the transient child window is minimized before its
  // parent window, its visibility is not controlled by its parent anymore.
  // Check |TransientWindowManager::UpdateTransientChildVisibility()| for more
  // details.
  const bool is_scaling_down =
      scale_type_ == WindowScaleAnimation::WindowScaleType::kScaleDownToShelf;
  for (auto* transient_window : GetTransientTreeIterator(window)) {
    if (transient_window->is_destroying())
      continue;

    if (is_scaling_down) {
      // Minimize the dragged window after transform animation is completed.
      window_util::MinimizeAndHideWithoutAnimation({transient_window});
      // Reset its transform to identity transform and its original backdrop
      // mode.
      transient_window->layer()->SetTransform(gfx::Transform());
      transient_window->layer()->SetOpacity(1.f);
    }
    WindowBackdrop::Get(transient_window)->RestoreBackdrop();
  }
}

}  // namespace ash

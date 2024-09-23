// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_animator_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/utility/layer_copy_animator.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_animations.h"
#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Returns the primary root window's container.
aura::Window* GetWallpaper() {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  return Shell::GetContainer(root_window, kShellWindowId_WallpaperContainer);
}

// Fades |window| to |opacity| over |duration|.
void StartOpacityAnimationForWindow(aura::Window* window,
                                    float opacity,
                                    base::TimeDelta duration,
                                    ui::LayerAnimationObserver* observer) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  ui::LayerAnimationSequence* sequence = new ui::LayerAnimationSequence(
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration));
  if (observer)
    sequence->AddObserver(observer);
  animator->StartAnimation(sequence);
}

// Makes |window| fully transparent instantaneously.
void HideWindowImmediately(aura::Window* window,
                           ui::LayerAnimationObserver* observer) {
  window->layer()->SetOpacity(0.0);
  if (observer)
    observer->OnLayerAnimationEnded(NULL);
}

void HideWindow(aura::Window* window,
                base::TimeDelta duration,
                bool above,
                ui::LayerAnimationObserver* observer) {
  ui::Layer* layer = window->layer();
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());

  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(duration);

  settings.SetTweenType(gfx::Tween::EASE_OUT);
  SetTransformForScaleAnimation(
      layer, above ? LAYER_SCALE_ANIMATION_ABOVE : LAYER_SCALE_ANIMATION_BELOW);

  settings.SetTweenType(gfx::Tween::EASE_IN_OUT);
  layer->SetOpacity(0.0f);

  // After the animation completes snap the transform back to the identity,
  // otherwise any one that asks for screen bounds gets a slightly scaled
  // version.
  settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
  settings.SetTransitionDuration(base::TimeDelta());
  layer->SetTransform(gfx::Transform());

  // A bit of a dirty trick: we need to catch the end of the animation we don't
  // control. So we use two facts we know: which animator will be used and the
  // target opacity to add "Do nothing" animation sequence.
  // Unfortunately, we can not just use empty LayerAnimationSequence, because
  // it does not call NotifyEnded().
  if (observer) {
    ui::LayerAnimationSequence* sequence = new ui::LayerAnimationSequence(
        ui::LayerAnimationElement::CreateOpacityElement(0.0,
                                                        base::TimeDelta()));
    sequence->AddObserver(observer);
    layer->GetAnimator()->ScheduleAnimation(sequence);
  }
}

// Animates |layer| to identity transform and full opacity over |duration|.
void TransformLayerToBaseState(ui::Layer* layer,
                               base::TimeDelta duration,
                               ui::LayerAnimationObserver* observer) {
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());

  // Animate to target values.
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(duration);

  settings.SetTweenType(gfx::Tween::EASE_OUT);
  layer->SetTransform(gfx::Transform());

  // TODO(oshima): TweenType can't be changed per property.
  settings.SetTweenType(gfx::Tween::EASE_IN_OUT);
  layer->SetOpacity(1.0f);

  // A bit of a dirty trick: we need to catch the end of the animation we don't
  // control. So we use two facts we know: which animator will be used and the
  // target opacity to add "Do nothing" animation sequence.
  // Unfortunately, we can not just use empty LayerAnimationSequence, because
  // it does not call NotifyEnded().
  if (observer) {
    ui::LayerAnimationSequence* sequence = new ui::LayerAnimationSequence(
        ui::LayerAnimationElement::CreateOpacityElement(1.0,
                                                        base::TimeDelta()));
    sequence->AddObserver(observer);
    layer->GetAnimator()->ScheduleAnimation(sequence);
  }
}

// Starts grayscale/brightness animation for |window| over |duration|. Target
// value for both grayscale and brightness are specified by |target|.
void StartGrayscaleBrightnessAnimationForWindow(
    aura::Window* window,
    float target,
    base::TimeDelta duration,
    gfx::Tween::Type tween_type,
    ui::LayerAnimationObserver* observer) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();

  std::unique_ptr<ui::LayerAnimationSequence> brightness_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  std::unique_ptr<ui::LayerAnimationSequence> grayscale_sequence =
      std::make_unique<ui::LayerAnimationSequence>();

  std::unique_ptr<ui::LayerAnimationElement> brightness_element =
      ui::LayerAnimationElement::CreateBrightnessElement(target, duration);
  brightness_element->set_tween_type(tween_type);
  brightness_sequence->AddElement(std::move(brightness_element));

  std::unique_ptr<ui::LayerAnimationElement> grayscale_element =
      ui::LayerAnimationElement::CreateGrayscaleElement(target, duration);
  grayscale_element->set_tween_type(tween_type);
  grayscale_sequence->AddElement(std::move(grayscale_element));

  std::vector<ui::LayerAnimationSequence*> animations;
  animations.push_back(brightness_sequence.release());
  animations.push_back(grayscale_sequence.release());

  if (observer)
    animations[0]->AddObserver(observer);

  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  animator->StartTogether(animations);
}

// Animation observer that will drop animated foreground once animation is
// finished. It is used in when undoing shutdown animation.
class CallbackAnimationObserver : public ui::LayerAnimationObserver {
 public:
  explicit CallbackAnimationObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  CallbackAnimationObserver(const CallbackAnimationObserver&) = delete;
  CallbackAnimationObserver& operator=(const CallbackAnimationObserver&) =
      delete;

  ~CallbackAnimationObserver() override = default;

 private:
  // Overridden from ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* seq) override {
    // Drop foreground once animation is over.
    std::move(callback_).Run();
    delete this;
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* seq) override {
    // Drop foreground once animation is over.
    std::move(callback_).Run();
    delete this;
  }

  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* seq) override {}

  base::OnceClosure callback_;
};

void GetContainersInRootWindow(int container_mask,
                               aura::Window* root_window,
                               aura::Window::Windows* containers) {
  if (container_mask & SessionStateAnimator::ROOT_CONTAINER) {
    containers->push_back(root_window);
  }

  if (container_mask & SessionStateAnimator::WALLPAPER) {
    containers->push_back(
        Shell::GetContainer(root_window, kShellWindowId_WallpaperContainer));
  }
  if (container_mask & SessionStateAnimator::SHELF) {
    containers->push_back(
        Shell::GetContainer(root_window, kShellWindowId_ShelfContainer));
  }
  if (container_mask & SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS) {
    // `non_lock_screen_containers` may already be removed in some tests.
    if (aura::Window* non_lock_screen_containers = Shell::GetContainer(
            root_window, kShellWindowId_NonLockScreenContainersContainer);
        non_lock_screen_containers) {
      for (const int id : SessionStateAnimatorImpl::
               ContainersToAnimateInNonLockScreenContainer) {
        containers->push_back(Shell::GetContainer(root_window, id));
      }
      // The active desk container should be animated as well besides the ones
      // inside `ContainersToAnimateInNonLockScreenContainer`.
      containers->push_back(
          desks_util::GetActiveDeskContainerForRoot(root_window));
    }
  }
  if (container_mask & SessionStateAnimator::LOCK_SCREEN_WALLPAPER) {
    containers->push_back(Shell::GetContainer(
        root_window, kShellWindowId_LockScreenWallpaperContainer));
  }
  if (container_mask & SessionStateAnimator::LOCK_SCREEN_CONTAINERS) {
    containers->push_back(Shell::GetContainer(
        root_window, kShellWindowId_LockScreenContainersContainer));
  }
  if (container_mask & SessionStateAnimator::LOCK_SCREEN_RELATED_CONTAINERS) {
    containers->push_back(Shell::GetContainer(
        root_window, kShellWindowId_LockScreenRelatedContainersContainer));
  }
}

void ShowWindow(aura::Window* window,
                base::TimeDelta duration,
                bool above,
                ui::LayerAnimationObserver* observer) {
  if (window->children().empty()) {
    window->layer()->SetTransform(gfx::Transform());
    window->layer()->SetOpacity(1.f);
    return;
  }
  auto* animator = LayerCopyAnimator::Get(window);
  if (!animator || animator->animation_requested())
    animator = new LayerCopyAnimator(window);

  auto animation_callback = [](base::TimeDelta duration, bool above,
                               ui::Layer* animating_layer,
                               ui::LayerAnimationObserver* observer) {
    DCHECK(animating_layer->parent());
    // Set initial state of animation
    SetTransformForScaleAnimation(
        animating_layer,
        above ? LAYER_SCALE_ANIMATION_ABOVE : LAYER_SCALE_ANIMATION_BELOW);
    animating_layer->SetOpacity(0.f);
    TransformLayerToBaseState(animating_layer, duration, observer);
  };
  animator->MaybeStartAnimation(
      observer, base::BindOnce(animation_callback, duration, above));
}

}  // namespace

// This observer is intended to use in cases when some action has to be taken
// once some animation successfully completes (i.e. it was not aborted).
// Observer will count a number of sequences it is attached to, and a number of
// finished sequences (either Ended or Aborted). Once these two numbers are
// equal, observer will delete itself, calling callback passed to constructor if
// there were no aborted animations.
// This way it can be either used to wait for some animation to be finished in
// multiple layers, to wait once a sequence of animations is finished in one
// layer or the mixture of both.
class SessionStateAnimatorImpl::AnimationSequence
    : public SessionStateAnimator::AnimationSequence,
      public ui::LayerAnimationObserver {
 public:
  explicit AnimationSequence(SessionStateAnimatorImpl* animator,
                             AnimationCallback callback)
      : SessionStateAnimator::AnimationSequence(std::move(callback)),
        animator_(animator),
        sequences_attached_(0),
        sequences_completed_(0) {}

  AnimationSequence(const AnimationSequence&) = delete;
  AnimationSequence& operator=(const AnimationSequence&) = delete;

  // SessionStateAnimator::AnimationSequence:
  void StartAnimation(int container_mask,
                      SessionStateAnimator::AnimationType type,
                      SessionStateAnimator::AnimationSpeed speed) override {
    animator_->StartAnimationInSequence(container_mask, type, speed, this);
  }

  void EndSequence() override {
    SessionStateAnimator::AnimationSequence::EndSequence();

    // Mark animation completed if there are no pending ones at the end in
    // case it is skipped during animation setup because the sequence is not
    // marked as ended..
    if (sequences_completed_ == sequences_attached_) {
      OnAnimationCompleted();
    }
  }

 private:
  ~AnimationSequence() override = default;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    sequences_completed_++;
    if (sequence_ended() && sequences_completed_ == sequences_attached_) {
      OnAnimationCompleted();
    }
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    sequences_completed_++;
    if (sequence_ended() && sequences_completed_ == sequences_attached_) {
      OnAnimationAborted();
    }
  }

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void OnAttachedToSequence(ui::LayerAnimationSequence* sequence) override {
    LayerAnimationObserver::OnAttachedToSequence(sequence);
    sequences_attached_++;
  }

  raw_ptr<SessionStateAnimatorImpl, LeakedDanglingUntriaged>
      animator_;  // not owned

  // Number of sequences this observer was attached to.
  int sequences_attached_;

  // Number of sequences either ended or aborted.
  int sequences_completed_;
};

SessionStateAnimatorImpl::SessionStateAnimatorImpl() = default;

SessionStateAnimatorImpl::~SessionStateAnimatorImpl() = default;

// Fills |containers| with the containers described by |container_mask|.
void SessionStateAnimatorImpl::GetContainers(
    int container_mask,
    aura::Window::Windows* containers) {
  containers->clear();

  for (aura::Window* root_window : Shell::GetAllRootWindows())
    GetContainersInRootWindow(container_mask, root_window, containers);

  // Some of containers may be null in some tests.
  containers->erase(
      std::remove(containers->begin(), containers->end(), nullptr),
      containers->end());
}

void SessionStateAnimatorImpl::StartAnimation(int container_mask,
                                              AnimationType type,
                                              AnimationSpeed speed) {
  aura::Window::Windows containers;
  GetContainers(container_mask, &containers);
  for (aura::Window::Windows::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    RunAnimationForWindow(*it, type, speed, NULL);
  }
}

void SessionStateAnimatorImpl::StartAnimationWithCallback(
    int container_mask,
    AnimationType type,
    AnimationSpeed speed,
    base::OnceClosure callback) {
  aura::Window::Windows containers;
  GetContainers(container_mask, &containers);
  base::RepeatingClosure animation_done_closure =
      base::BarrierClosure(containers.size(), std::move(callback));
  for (aura::Window::Windows::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    ui::LayerAnimationObserver* observer =
        new CallbackAnimationObserver(animation_done_closure);
    RunAnimationForWindow(*it, type, speed, observer);
  }
}

SessionStateAnimator::AnimationSequence*
SessionStateAnimatorImpl::BeginAnimationSequence(AnimationCallback callback) {
  return new AnimationSequence(this, std::move(callback));
}

bool SessionStateAnimatorImpl::IsWallpaperHidden() const {
  return !GetWallpaper()->IsVisible();
}

void SessionStateAnimatorImpl::ShowWallpaper() {
  ui::ScopedLayerAnimationSettings settings(
      GetWallpaper()->layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta());
  GetWallpaper()->Show();
}

void SessionStateAnimatorImpl::HideWallpaper() {
  ui::ScopedLayerAnimationSettings settings(
      GetWallpaper()->layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta());
  GetWallpaper()->Hide();
}

void SessionStateAnimatorImpl::StartAnimationInSequence(
    int container_mask,
    AnimationType type,
    AnimationSpeed speed,
    AnimationSequence* observer) {
  aura::Window::Windows containers;
  GetContainers(container_mask, &containers);
  for (aura::Window::Windows::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    RunAnimationForWindow(*it, type, speed, observer);
  }
}

void SessionStateAnimatorImpl::RunAnimationForWindow(
    aura::Window* window,
    AnimationType type,
    AnimationSpeed speed,
    ui::LayerAnimationObserver* observer) {
  base::TimeDelta duration = GetDuration(speed);

  switch (type) {
    case ANIMATION_FADE_IN:
      StartOpacityAnimationForWindow(window, 1.0, duration, observer);
      break;
    case ANIMATION_FADE_OUT:
      StartOpacityAnimationForWindow(window, 0.0, duration, observer);
      break;
    case ANIMATION_HIDE_IMMEDIATELY:
      DCHECK_EQ(speed, ANIMATION_SPEED_IMMEDIATE);
      HideWindowImmediately(window, observer);
      break;
    case ANIMATION_LIFT:
      HideWindow(window, duration, true, observer);
      break;
    case ANIMATION_DROP:
      ShowWindow(window, duration, true, observer);
      break;
    case ANIMATION_UNDO_LIFT:
      TransformLayerToBaseState(window->layer(), duration, observer);
      break;
    case ANIMATION_RAISE_TO_SCREEN:
      ShowWindow(window, duration, false, observer);
      break;
    case ANIMATION_GRAYSCALE_BRIGHTNESS:
      StartGrayscaleBrightnessAnimationForWindow(window, 1.0, duration,
                                                 gfx::Tween::EASE_IN, observer);
      break;
    case ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS:
      StartGrayscaleBrightnessAnimationForWindow(
          window, 0.0, duration, gfx::Tween::EASE_IN_OUT, observer);
      break;
    case ANIMATION_COPY_LAYER:
      if (!window->children().empty())
        new LayerCopyAnimator(window);
      break;
  }
}

}  // namespace ash

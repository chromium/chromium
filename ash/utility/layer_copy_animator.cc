// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/layer_copy_animator.h"

#include "ash/utility/layer_util.h"
#include "base/functional/bind.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::LayerCopyAnimator*)

namespace ash {
namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(LayerCopyAnimator,
                                   kLayerCopyAnimatorKey,
                                   nullptr)

// CopyOutputRequest's callback may be called on the different thread during
// shutdown, which results in the DCHECK failure in the weak ptr when
// referenced.
void MaybeLayerCopied(base::WeakPtr<LayerCopyAnimator> swc,
                      std::unique_ptr<ui::Layer> new_layer) {
  if (swc) {
    swc->OnLayerCopied(std::move(new_layer));
  }
}

}  // namespace

// static
LayerCopyAnimator* LayerCopyAnimator::Get(aura::Window* window) {
  return window->GetProperty(kLayerCopyAnimatorKey);
}

LayerCopyAnimator::LayerCopyAnimator(aura::Window* window) : window_(window) {
  window->SetProperty(kLayerCopyAnimatorKey, this);
  observation_.Observe(window);

  // Copy request will not copy NOT_DRAWN and the result may be smaller than
  // requested layer.  Create a transparent layer to cover the entire layer.
  if (window_->layer()->type() == ui::LAYER_NOT_DRAWN) {
    full_layer_.SetColor(SK_ColorTRANSPARENT);
    full_layer_.SetBounds(gfx::Rect(window_->bounds().size()));
    window_->layer()->Add(&full_layer_);
    window_->layer()->StackAtBottom(&full_layer_);
  }
  window_->layer()->GetAnimator()->StopAnimating();
  window_->layer()->SetOpacity(1.f);

  CopyLayerContentToNewLayer(
      window_->layer(),
      base::BindOnce(&MaybeLayerCopied, weak_ptr_factory_.GetWeakPtr()));
}

LayerCopyAnimator::~LayerCopyAnimator() {
  window_->layer()->SetOpacity(1.0f);
  if (fake_sequence_)
    NotifyWithFakeSequence(/*abort=*/true);
  DCHECK(!observer_);
}

void LayerCopyAnimator::MaybeStartAnimation(
    ui::LayerAnimationObserver* observer,
    AnimationCallback callback) {
  DCHECK(!animation_requested_);
  observer_ = observer;
  animation_callback_ = std::move(callback);
  animation_requested_ = true;
  if (fail_) {
    FinishAndDelete(/*abort=*/true);
    return;
  }
  if (copied_layer_) {
    RunAnimation();
    return;
  }
  if (observer_)
    EnsureFakeSequence();
}

void LayerCopyAnimator::OnLayerCopied(std::unique_ptr<ui::Layer> new_layer) {
  if (fail_)
    return;

  if (!new_layer)
    fail_ = true;

  copied_layer_ = std::move(new_layer);
  if (animation_requested_ && !fail_)
    RunAnimation();
}

void LayerCopyAnimator::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  if (last_sequence_ == sequence)
    window_->ClearProperty(kLayerCopyAnimatorKey);
}

void LayerCopyAnimator::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  window_->ClearProperty(kLayerCopyAnimatorKey);
}

void LayerCopyAnimator::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds,
                                              ui::PropertyChangeReason reason) {
  fail_ = true;
  if (copied_layer_)
    copied_layer_->GetAnimator()->StopAnimating();
}

void LayerCopyAnimator::RunAnimation() {
  copied_layer_->SetFillsBoundsOpaquely(false);

  auto* parent_layer = window_->layer()->parent();
  parent_layer->Add(copied_layer_.get());
  parent_layer->StackAbove(copied_layer_.get(), window_->layer());
  window_->layer()->SetOpacity(0.f);

  std::move(animation_callback_).Run(copied_layer_.get(), observer_.get());

  // Callback may not run animations, in which case, just end immediately.
  if (!copied_layer_->GetAnimator()->is_animating()) {
    FinishAndDelete(/*abort=*/false);
    return;
  }

  if (fake_sequence_)
    NotifyWithFakeSequence(/*abort=*/false);

  observer_ = nullptr;
  last_sequence_ = new ui::LayerAnimationSequence(
      ui::LayerAnimationElement::CreateOpacityElement(1.0, base::TimeDelta()));
  copied_layer_->GetAnimator()->ScheduleAnimation(last_sequence_);
  copied_layer_->GetAnimator()->AddObserver(this);
}

void LayerCopyAnimator::FinishAndDelete(bool abort) {
  if (observer_) {
    EnsureFakeSequence();
    NotifyWithFakeSequence(abort);
  }
  window_->ClearProperty(kLayerCopyAnimatorKey);
}

void LayerCopyAnimator::NotifyWithFakeSequence(bool abort) {
  DCHECK(fake_sequence_);
  if (abort)
    observer_->OnLayerAnimationAborted(fake_sequence_.get());
  else
    observer_->OnLayerAnimationEnded(fake_sequence_.get());
  fake_sequence_.reset();
  observer_ = nullptr;
}

void LayerCopyAnimator::EnsureFakeSequence() {
  if (fake_sequence_)
    return;
  fake_sequence_ = std::make_unique<ui::LayerAnimationSequence>(
      ui::LayerAnimationElement::CreateOpacityElement(0.0, base::TimeDelta()));
  fake_sequence_->AddObserver(observer_);
}

}  // namespace ash

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_ui_element_view_animator.h"

#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

namespace {

using assistant::util::CreateLayerAnimationSequence;
using assistant::util::CreateOpacityElement;
using assistant::util::StartLayerAnimationSequence;

// Animation.
constexpr base::TimeDelta kAnimateInDuration = base::Milliseconds(250);
constexpr base::TimeDelta kAnimateOutDuration = base::Milliseconds(200);

}  // namespace

AssistantUiElementViewAnimator::AssistantUiElementViewAnimator(
    AssistantUiElementView* view,
    const char* animation_smoothness_histogram)
    : ElementAnimator(view),
      view_(view),
      animation_smoothness_histogram_(animation_smoothness_histogram) {}

AssistantUiElementViewAnimator::AnimationSmoothnessCallback
AssistantUiElementViewAnimator::GetAnimationSmoothnessCallback() const {
  return base::BindRepeating<void(const std::string&, int value)>(
      base::UmaHistogramPercentage, animation_smoothness_histogram_);
}

// ElementAnimator:
void AssistantUiElementViewAnimator::AnimateIn(
    ui::CallbackLayerAnimationObserver* observer) {
  StartLayerAnimationSequence(
      layer()->GetAnimator(),
      CreateLayerAnimationSequence(CreateOpacityElement(
          1.f, kAnimateInDuration, gfx::Tween::Type::FAST_OUT_SLOW_IN)),
      observer, GetAnimationSmoothnessCallback());
}

void AssistantUiElementViewAnimator::AnimateOut(
    ui::CallbackLayerAnimationObserver* observer) {
  StartLayerAnimationSequence(
      layer()->GetAnimator(),
      CreateLayerAnimationSequence(
          CreateOpacityElement(kMinimumAnimateOutOpacity, kAnimateOutDuration)),
      observer, GetAnimationSmoothnessCallback());
}

ui::Layer* AssistantUiElementViewAnimator::layer() const {
  return view_->GetLayerForAnimating();
}

}  // namespace ash

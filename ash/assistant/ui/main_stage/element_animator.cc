// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/element_animator.h"

#include "ash/assistant/util/animation_util.h"
#include "base/time/time.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/view.h"

namespace ash {

constexpr float ElementAnimator::kFadeOutOpacity;
constexpr base::TimeDelta ElementAnimator::kFadeOutDuration;
constexpr float ElementAnimator::kMinimumAnimateOutOpacity;

ElementAnimator::ElementAnimator(views::View* view) : view_(view) {}

void ElementAnimator::FadeOut(ui::CallbackLayerAnimationObserver* observer) {
  assistant::util::StartLayerAnimationSequence(
      layer()->GetAnimator(),
      assistant::util::CreateLayerAnimationSequence(
          assistant::util::CreateOpacityElement(kFadeOutOpacity,
                                                kFadeOutDuration)),
      observer);
}

void ElementAnimator::AbortAnimation() {
  layer()->GetAnimator()->AbortAllAnimations();
}

views::View* ElementAnimator::view() const {
  return view_;
}

ui::Layer* ElementAnimator::layer() const {
  return view()->layer();
}

}  // namespace ash

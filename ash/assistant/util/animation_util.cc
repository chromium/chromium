// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/animation_util.h"

#include "ash/public/cpp/metrics_util.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/view.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

// Returns an observer that will hide |view| when it fires.
// The observer will delete itself after firing (by returning true).
ui::CallbackLayerAnimationObserver* BuildObserverToHideView(views::View* view) {
  return new ui::CallbackLayerAnimationObserver(base::BindRepeating(
      /*animation_ended_callback=*/
      [](views::View* view,
         const ui::CallbackLayerAnimationObserver& observer) {
        // If the animation was aborted, we just return true to delete our
        // observer. No further action is needed, as |view| might no longer
        // be valid.
        if (observer.aborted_count())
          return true;

        view->SetVisible(false);
        // We return true to delete our observer.
        return true;
      },
      view));
}

}  // namespace

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    const LayerAnimationSequenceParams& params) {
  return CreateLayerAnimationSequence(std::move(a), nullptr, nullptr, nullptr,
                                      params);
}

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    const LayerAnimationSequenceParams& params) {
  return CreateLayerAnimationSequence(std::move(a), std::move(b), nullptr,
                                      nullptr, params);
}

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    std::unique_ptr<::ui::LayerAnimationElement> c,
    const LayerAnimationSequenceParams& params) {
  return CreateLayerAnimationSequence(std::move(a), std::move(b), std::move(c),
                                      nullptr, params);
}

::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    std::unique_ptr<::ui::LayerAnimationElement> c,
    std::unique_ptr<::ui::LayerAnimationElement> d,
    const LayerAnimationSequenceParams& params) {
  ui::LayerAnimationSequence* layer_animation_sequence =
      new ui::LayerAnimationSequence();

  layer_animation_sequence->AddElement(std::move(a));

  if (b)
    layer_animation_sequence->AddElement(std::move(b));

  if (c)
    layer_animation_sequence->AddElement(std::move(c));

  if (d)
    layer_animation_sequence->AddElement(std::move(d));

  layer_animation_sequence->set_is_repeating(params.is_cyclic);

  return layer_animation_sequence;
}

std::unique_ptr<::ui::LayerAnimationElement> CreateOpacityElement(
    float opacity,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween) {
  std::unique_ptr<::ui::LayerAnimationElement> layer_animation_element =
      ::ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  layer_animation_element->set_tween_type(tween);
  return layer_animation_element;
}

std::unique_ptr<::ui::LayerAnimationElement> CreateTransformElement(
    const gfx::Transform& transform,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween) {
  std::unique_ptr<::ui::LayerAnimationElement> layer_animation_element =
      ::ui::LayerAnimationElement::CreateTransformElement(transform, duration);
  layer_animation_element->set_tween_type(tween);
  return layer_animation_element;
}

void StartLayerAnimationSequence(
    ::ui::LayerAnimator* layer_animator,
    ::ui::LayerAnimationSequence* layer_animation_sequence,
    ::ui::LayerAnimationObserver* observer,
    std::optional<AnimationSmoothnessCallback> smoothness_callback) {
  if (observer)
    layer_animation_sequence->AddObserver(observer);

  std::optional<ui::AnimationThroughputReporter> reporter;
  if (smoothness_callback) {
    reporter.emplace(layer_animator, ash::metrics_util::ForSmoothnessV3(
                                         smoothness_callback.value()));
  }
  layer_animator->StartAnimation(layer_animation_sequence);
}

void StartLayerAnimationSequence(
    views::View* view,
    ::ui::LayerAnimationSequence* layer_animation_sequence,
    ::ui::LayerAnimationObserver* observer,
    std::optional<AnimationSmoothnessCallback> smoothness_callback) {
  DCHECK(view->layer());
  StartLayerAnimationSequence(view->layer()->GetAnimator(),
                              layer_animation_sequence, observer,
                              smoothness_callback);
}

void StartLayerAnimationSequencesTogether(
    ::ui::LayerAnimator* layer_animator,
    const std::vector<ui::LayerAnimationSequence*>& layer_animation_sequences,
    ::ui::LayerAnimationObserver* observer) {
  if (observer) {
    for (::ui::LayerAnimationSequence* layer_animation_sequence :
         layer_animation_sequences) {
      layer_animation_sequence->AddObserver(observer);
    }
  }
  layer_animator->StartTogether(layer_animation_sequences);
}

void FadeOutAndHide(views::View* view, base::TimeDelta fade_out_duration) {
  // Note: We are deliberately not simply setting the layer's visibility to
  // false by ending the animation with a |CreateVisibilityElement|.
  // The reason is that this would hide the layer but not the view, leaving the
  // view in the accessibility tree, causing issues like b/142672872.

  auto* animation_observer = BuildObserverToHideView(view);

  StartLayerAnimationSequence(view,
                              CreateLayerAnimationSequence(
                                  CreateOpacityElement(0.f, fade_out_duration)),
                              animation_observer);

  animation_observer->SetActive();
}

}  // namespace util
}  // namespace assistant
}  // namespace ash

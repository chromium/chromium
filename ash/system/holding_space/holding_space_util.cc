// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_util.h"

#include <memory>
#include <optional>

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/view.h"

namespace ash::holding_space_util {

namespace {

// CallbackPathGenerator -------------------------------------------------------

class CallbackPathGenerator : public views::HighlightPathGenerator {
 public:
  using Callback = base::RepeatingCallback<gfx::RRectF()>;

  explicit CallbackPathGenerator(Callback callback)
      : callback_(std::move(callback)) {}
  CallbackPathGenerator(const CallbackPathGenerator&) = delete;
  CallbackPathGenerator& operator=(const CallbackPathGenerator&) = delete;
  ~CallbackPathGenerator() override = default;

 private:
  // views::HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return callback_.Run();
  }

  Callback callback_;
};

// Helpers ---------------------------------------------------------------------

// Creates a `ui::LayerAnimationSequence` for the specified `element` with
// optional `delay`, observed by the specified `observer`.
std::unique_ptr<ui::LayerAnimationSequence> CreateObservedSequence(
    std::unique_ptr<ui::LayerAnimationElement> element,
    base::TimeDelta delay,
    ui::LayerAnimationObserver* observer) {
  auto sequence = std::make_unique<ui::LayerAnimationSequence>();
  if (!delay.is_zero()) {
    sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
        element->properties(), delay));
  }
  sequence->AddElement(std::move(element));
  sequence->AddObserver(observer);
  return sequence;
}

// Animates the specified `view` to a target `opacity` with the specified
// `duration` and optional `delay`, associating `observer` with the created
// animation sequences.
void AnimateTo(views::View* view,
               float opacity,
               base::TimeDelta duration,
               base::TimeDelta delay,
               ui::LayerAnimationObserver* observer) {
  // Opacity animation.
  auto opacity_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  opacity_element->set_tween_type(gfx::Tween::Type::LINEAR);

  // Note that the `ui::LayerAnimator` takes ownership of any animation
  // sequences so they need to be released.
  view->layer()->GetAnimator()->StartAnimation(
      CreateObservedSequence(std::move(opacity_element), delay, observer)
          .release());
}

}  // namespace

// Animates in the specified `view` with the specified `duration` and optional
// `delay`, associating `observer` with the created animation sequences.
void AnimateIn(views::View* view,
               base::TimeDelta duration,
               base::TimeDelta delay,
               ui::LayerAnimationObserver* observer) {
  view->layer()->SetOpacity(0.f);
  AnimateTo(view, /*opacity=*/1.f, duration, delay, observer);
}

// Animates out the specified `view` with the specified `duration, associating
// `observer` with the created animation sequences.
void AnimateOut(views::View* view,
                base::TimeDelta duration,
                ui::LayerAnimationObserver* observer) {
  AnimateTo(view, /*opacity=*/0.f, duration, /*delay=*/base::TimeDelta(),
            observer);
}

std::unique_ptr<views::HighlightPathGenerator> CreateHighlightPathGenerator(
    base::RepeatingCallback<gfx::RRectF()> callback) {
  return std::make_unique<CallbackPathGenerator>(std::move(callback));
}

}  // namespace ash::holding_space_util

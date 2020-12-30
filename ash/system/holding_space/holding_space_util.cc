// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_util.h"

#include "ash/style/ash_color_provider.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace holding_space_util {

namespace {

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

std::unique_ptr<views::Label> CreateLabel(LabelStyle style,
                                          const base::string16& text) {
  auto label = std::make_unique<views::Label>(text);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  switch (style) {
    case LabelStyle::kBadge:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case LabelStyle::kBody:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::NORMAL));
      break;
    case LabelStyle::kChip:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 13,
                                       gfx::Font::Weight::NORMAL));
      break;
    case LabelStyle::kHeader:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 16,
                                       gfx::Font::Weight::MEDIUM));
      break;
  }

  return label;
}

}  // namespace holding_space_util
}  // namespace ash

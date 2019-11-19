// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_
#define ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "ui/gfx/animation/tween.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ui {
class LayerAnimationElement;
class LayerAnimationObserver;
class LayerAnimationSequence;
class LayerAnimator;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {
namespace assistant {
namespace util {

// Parameters for a LayerAnimationSequence.
struct LayerAnimationSequenceParams {
  // True if the animation sequence should loop endlessly, false otherwise.
  bool is_cyclic = false;
};

// Creates a LayerAnimationSequence containing the specified
// LayerAnimationElements with the given |params|. The method caller assumes
// ownership of the returned pointer.
COMPONENT_EXPORT(ASSISTANT_UTIL)
::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    const LayerAnimationSequenceParams& params = {});

// Creates a LayerAnimationSequence containing the specified
// LayerAnimationElements with the given |params|. The method caller assumes
// ownership of the returned pointer.
COMPONENT_EXPORT(ASSISTANT_UTIL)
::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    const LayerAnimationSequenceParams& params = {});

// Creates a LayerAnimationSequence containing the specified
// LayerAnimationElements with the given |params|. The method caller assumes
// ownership of the returned pointer.
COMPONENT_EXPORT(ASSISTANT_UTIL)
::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    std::unique_ptr<::ui::LayerAnimationElement> c,
    const LayerAnimationSequenceParams& params = {});

// Creates a LayerAnimationSequence containing the specified
// LayerAnimationElements with the given |params|. The method caller assumes
// ownership of the returned pointer.
COMPONENT_EXPORT(ASSISTANT_UTIL)
::ui::LayerAnimationSequence* CreateLayerAnimationSequence(
    std::unique_ptr<::ui::LayerAnimationElement> a,
    std::unique_ptr<::ui::LayerAnimationElement> b,
    std::unique_ptr<::ui::LayerAnimationElement> c,
    std::unique_ptr<::ui::LayerAnimationElement> d,
    const LayerAnimationSequenceParams& params = {});

// Creates a LayerAnimationElement to animate opacity with the given parameters.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::unique_ptr<::ui::LayerAnimationElement> CreateOpacityElement(
    float opacity,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween = gfx::Tween::Type::LINEAR);

// Creates a LayerAnimationElement to animate transform with the given
// parameters.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::unique_ptr<::ui::LayerAnimationElement> CreateTransformElement(
    const gfx::Transform& transform,
    const base::TimeDelta& duration,
    const gfx::Tween::Type& tween = gfx::Tween::Type::LINEAR);

// Starts the specified |layer_animation_sequence| on the given
// |layer_animator|. If an optional |observer| is supplied, it will be added to
// the sequence.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void StartLayerAnimationSequence(
    ::ui::LayerAnimator* layer_animator,
    ::ui::LayerAnimationSequence* layer_animation_sequence,
    ::ui::LayerAnimationObserver* observer = nullptr);

// Starts the specified |layer_animation_sequence| on the layer of the given
// |view|. If an optional |observer| is supplied, it will be added to the
// sequence.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void StartLayerAnimationSequence(
    views::View* view,
    ::ui::LayerAnimationSequence* layer_animation_sequence,
    ::ui::LayerAnimationObserver* observer = nullptr);

// Starts the specified |layer_animation_sequences| together on the given
// |layer_animator|. If an optional |observer| is supplied, it will be added
// to each sequence in the animation set.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void StartLayerAnimationSequencesTogether(
    ::ui::LayerAnimator* layer_animator,
    const std::vector<::ui::LayerAnimationSequence*>& layer_animation_sequences,
    ::ui::LayerAnimationObserver* observer = nullptr);

// Starts the animation to fade out the given view, and hide it when the fadeout
// is completed.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void FadeOutAndHide(views::View* view, base::TimeDelta fade_out_duration);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_ANIMATION_UTIL_H_

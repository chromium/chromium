// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/animated_paint_worklet_tracker.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/layers/picture_layer_impl.h"

namespace cc {

AnimatedPaintWorkletTracker::AnimatedPaintWorkletTracker() = default;

AnimatedPaintWorkletTracker::~AnimatedPaintWorkletTracker() = default;

AnimatedPaintWorkletTracker::PropertyState::PropertyState(PropertyValue value)
    : animation_value(value) {}

AnimatedPaintWorkletTracker::PropertyState::PropertyState() = default;

AnimatedPaintWorkletTracker::PropertyState::PropertyState(
    const PropertyState& other) = default;

AnimatedPaintWorkletTracker::PropertyState::~PropertyState() = default;

void AnimatedPaintWorkletTracker::OnCustomPropertyMutated(
    PropertyKey property_key,
    PropertyValue property_value) {
  auto iter = input_properties_.find(property_key);
  // OnCustomPropertyMutated is called for all composited custom property
  // animations and some types of native properties that uses the paint worklet
  // infra, but there may not be a matching PaintWorklet, and thus no entry
  // in |input_properties_|.
  // TODO(xidachen): Only create composited custom property animations if they
  // affect paint worklet.
  if (iter == input_properties_.end()) {
    return;
  }
  iter->second.animation_value = std::move(property_value);
  // Keep track of which input properties have been changed so that the
  // associated PaintWorklets can be invalidated before activating the pending
  // tree.
  input_properties_animated_on_impl_.insert(property_key);
}

AnimatedPaintWorkletTracker::PropertyChangeMap
AnimatedPaintWorkletTracker::TakeAndResetAnimatedProperties() {
  PropertyChangeMap result;
  for (const auto& prop_key : input_properties_animated_on_impl_) {
    auto it = input_properties_.find(prop_key);
    // Since the invalidations happen on a newly created pending tree,
    // previously animated input properties may not exist on this tree.
    if (it == input_properties_.end()) {
      continue;
    }
    PropertyState& state = it->second;
    result.insert(std::make_pair(
        prop_key,
        std::make_pair(state.last_animation_value, state.animation_value)));
    state.last_animation_value = state.animation_value;
  }
  input_properties_animated_on_impl_.clear();
  return result;
}

PaintWorkletInput::PropertyValue
AnimatedPaintWorkletTracker::GetPropertyAnimationValue(
    const PropertyKey& key) const {
  return input_properties_.find(key)->second.animation_value;
}

void AnimatedPaintWorkletTracker::UpdatePaintWorkletInputProperties(
    const std::vector<DiscardableImageMap::PaintWorkletInputWithImageId>&
        inputs) {
  for (const auto& input : inputs) {
    for (const auto& prop_key : input.first->GetPropertyKeys()) {
      input_properties_.try_emplace(prop_key);
    }
  }
}

void AnimatedPaintWorkletTracker::ClearUnusedInputProperties(
    base::flat_set<PropertyKey> used_properties) {
  // base::flat_map::erase takes linear time, which causes O(N^2) behavior when
  // using a naive loop + erase approach. Using base::EraseIf avoids that.
  base::EraseIf(
      input_properties_,
      [&used_properties](const std::pair<PropertyKey, PropertyState>& entry) {
        return !used_properties.contains(entry.first);
      });
  for (auto& entry : input_properties_)
    entry.second.animation_value.reset();
}

bool AnimatedPaintWorkletTracker::HasInputPropertiesAnimatedOnImpl() const {
  return !input_properties_animated_on_impl_.empty();
}

}  // namespace cc

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

AnimatedPaintWorkletTracker::PropertyState::PropertyState(
    PaintWorkletInput::PropertyValue value,
    base::flat_set<raw_ptr<PictureLayerImpl, CtnExperimental>> layers)
    : animation_value(value), associated_layers(std::move(layers)) {}

AnimatedPaintWorkletTracker::PropertyState::PropertyState() = default;

AnimatedPaintWorkletTracker::PropertyState::PropertyState(
    const PropertyState& other) = default;

AnimatedPaintWorkletTracker::PropertyState::~PropertyState() = default;

void AnimatedPaintWorkletTracker::OnCustomPropertyMutated(
    PaintWorkletInput::PropertyKey property_key,
    PaintWorkletInput::PropertyValue property_value) {
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

bool AnimatedPaintWorkletTracker::InvalidatePaintWorkletsOnPendingTree() {
  for (const auto& prop_key : input_properties_animated_on_impl_) {
    auto it = input_properties_.find(prop_key);
    // Since the invalidations happen on a newly created pending tree,
    // previously animated input properties may not exist on this tree.
    if (it == input_properties_.end()) {
      continue;
    }
    for (PictureLayerImpl* layer : it->second.associated_layers) {
      layer->InvalidatePaintWorklets(
          prop_key, input_properties_.find(prop_key)->second.animation_value,
          input_properties_.find(prop_key)->second.last_animation_value);
    }
    it->second.last_animation_value = it->second.animation_value;
  }
  bool return_value = !input_properties_animated_on_impl_.empty();
  input_properties_animated_on_impl_.clear();
  return return_value;
}

PaintWorkletInput::PropertyValue
AnimatedPaintWorkletTracker::GetPropertyAnimationValue(
    const PaintWorkletInput::PropertyKey& key) const {
  return input_properties_.find(key)->second.animation_value;
}

void AnimatedPaintWorkletTracker::UpdatePaintWorkletInputProperties(
    const std::vector<DiscardableImageMap::PaintWorkletInputWithImageId>&
        inputs,
    PictureLayerImpl* layer) {
  // Flatten the |inputs| into a set of all PropertyKeys, as we only care about
  // PropertyKeys and PictureLayerImpls.
  std::vector<PaintWorkletInput::PropertyKey> all_input_properties_vector;
  for (const auto& input : inputs) {
    all_input_properties_vector.insert(
        all_input_properties_vector.end(),
        std::begin(input.first->GetPropertyKeys()),
        std::end(input.first->GetPropertyKeys()));
  }
  base::flat_set<PaintWorkletInput::PropertyKey> all_input_properties(
      std::move(all_input_properties_vector));

  // Update all existing properties, marking whether or not the input |layer| is
  // associated with them.
  for (auto& entry : input_properties_) {
    if (all_input_properties.contains(entry.first))
      entry.second.associated_layers.insert(layer);
    else
      entry.second.associated_layers.erase(layer);
  }

  // Handle any new properties that we did not previously track.
  for (const auto& prop_key : all_input_properties) {
    if (!input_properties_.contains(prop_key))
      input_properties_[prop_key].associated_layers.insert(layer);
  }
}

void AnimatedPaintWorkletTracker::ClearUnusedInputProperties() {
  // base::flat_map::erase takes linear time, which causes O(N^2) behavior when
  // using a naive loop + erase approach. Using base::EraseIf avoids that.
  base::EraseIf(
      input_properties_,
      [](const std::pair<PaintWorkletInput::PropertyKey, PropertyState>&
             entry) { return entry.second.associated_layers.empty(); });
  for (auto& entry : input_properties_)
    entry.second.animation_value.reset();
}

}  // namespace cc

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_ANIMATED_PAINT_WORKLET_TRACKER_H_
#define CC_TREES_ANIMATED_PAINT_WORKLET_TRACKER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "cc/cc_export.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_worklet_input.h"

namespace cc {

class PictureLayerImpl;

// AnimatedPaintWorkletTracker is responsible for managing the state needed to
// hook up the compositor animation system to PaintWorklets. This allows a
// PaintWorklet to be animated (i.e. have its input values changed) entirely on
// the compositor without requiring a main frame commit.
//
// This class tracks all properties that any PaintWorklet depends on, whether or
// not they are animated by the compositor.
//
// At the current time only custom properties are supported.
class CC_EXPORT AnimatedPaintWorkletTracker {
 public:
  AnimatedPaintWorkletTracker();
  AnimatedPaintWorkletTracker(const AnimatedPaintWorkletTracker&) = delete;
  AnimatedPaintWorkletTracker& operator=(const AnimatedPaintWorkletTracker&) =
      delete;
  ~AnimatedPaintWorkletTracker();

  // Update the |input_properties_| map, which is a map from a property to all
  // picture layers that have a PaintWorkletInput that depends on the property.
  void UpdatePaintWorkletInputProperties(
      const std::vector<DiscardableImageMap::PaintWorkletInputWithImageId>&
          inputs,
      PictureLayerImpl* layer);

  // Called when the value of a property is changed by the CC animation system.
  // Responsible for updating the property value in |input_properties_|, and
  // marking any relevant PaintWorkletInputs as needs-invalidation.
  void OnCustomPropertyMutated(
      ElementId element_id,
      const std::string& custom_property_name,
      PaintWorkletInput::PropertyValue custom_property_value);
  // Invalidate all the paint worklets that uses the set of dirtied properties.
  // Returns whether the set of dirtied properties is empty or not.
  bool InvalidatePaintWorkletsOnPendingTree();

  // Given a property, return its latest value if the property is animated.
  // Otherwise return a PropertyValue with no value.
  PaintWorkletInput::PropertyValue GetPropertyAnimationValue(
      const PaintWorkletInput::PropertyKey& key) const;

  // Called right after Blink commit, clears the entries in |input_properties_|
  // that is never mutated by CC animations from the previous Blink commit. Also
  // reset the property value in the map.
  void ClearUnusedInputProperties();

 private:
  // PropertyState contains the state we track for each property that any
  // PaintWorklet depends on. This consists of:
  //
  //   1. The latest animation value if this property has been animated and is
  //      not independently tracked by property trees (i.e. opacity).
  //
  //   2. All the PictureLayerImpls which are associated (via a PaintWorklet)
  //      with the given property. This helps us to find all the
  //      PaintWorkletInputs that depend on the property, so that we can
  //      invalidate them when the property's value is changed by an animation.
  struct PropertyState {
    PropertyState();
    explicit PropertyState(PaintWorkletInput::PropertyValue value,
                           base::flat_set<PictureLayerImpl*> layers);
    PropertyState(const PropertyState&);
    ~PropertyState();

    // The values for all properties mutated by CC animations are set on every
    // commit. So we reset this value on every commit to identify properties
    // still being animated in CC. This allows us to continue using the final
    // value of the animation, after it finishes on the impl thread, until the
    // next commit.
    PaintWorkletInput::PropertyValue animation_value;
    base::flat_set<PictureLayerImpl*> associated_layers;
  };

  // The set of input properties managed by AnimatedPaintWorkletTracker.
  base::flat_map<PaintWorkletInput::PropertyKey, PropertyState>
      input_properties_;

  // Tracks the set of input properties that were invalidated by an animation
  // in the current frame. These are used to invalidate the relevant
  // PaintWorklets after either an impl side invalidation or commit, so that
  // they will be repainted. The set is emptied once all paint worklets have
  // been invalidated.
  base::flat_set<PaintWorkletInput::PropertyKey>
      input_properties_animated_on_impl_;
};

}  // namespace cc

#endif  // CC_TREES_ANIMATED_PAINT_WORKLET_TRACKER_H_

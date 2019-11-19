// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MUTATOR_HOST_CLIENT_H_
#define CC_TREES_MUTATOR_HOST_CLIENT_H_

#include "cc/paint/element_id.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/trees/property_animation_state.h"
#include "cc/trees/target_property.h"

namespace gfx {
class Transform;
class ScrollOffset;
}

namespace cc {

class FilterOperations;

enum class ElementListType { ACTIVE, PENDING };

enum class AnimationWorkletMutationState {
  STARTED,
  COMPLETED_WITH_UPDATE,
  COMPLETED_NO_UPDATE,
  CANCELED
};

class MutatorHostClient {
 public:
  virtual bool IsElementInPropertyTrees(ElementId element_id,
                                        ElementListType list_type) const = 0;

  virtual void SetMutatorsNeedCommit() = 0;
  virtual void SetMutatorsNeedRebuildPropertyTrees() = 0;

  virtual void SetElementFilterMutated(ElementId element_id,
                                       ElementListType list_type,
                                       const FilterOperations& filters) = 0;
  virtual void SetElementBackdropFilterMutated(
      ElementId element_id,
      ElementListType list_type,
      const FilterOperations& backdrop_filters) = 0;
  virtual void SetElementOpacityMutated(ElementId element_id,
                                        ElementListType list_type,
                                        float opacity) = 0;
  virtual void SetElementTransformMutated(ElementId element_id,
                                          ElementListType list_type,
                                          const gfx::Transform& transform) = 0;
  virtual void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) = 0;

  // Allows to change IsAnimating value for a set of properties.
  virtual void ElementIsAnimatingChanged(
      const PropertyToElementIdMap& element_id_map,
      ElementListType list_type,
      const PropertyAnimationState& mask,
      const PropertyAnimationState& state) = 0;

  virtual void AnimationScalesChanged(ElementId element_id,
                                      ElementListType list_type,
                                      float maximum_scale,
                                      float starting_scale) = 0;

  virtual void ScrollOffsetAnimationFinished() = 0;
  virtual gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const = 0;

  virtual void NotifyAnimationWorkletStateChange(
      AnimationWorkletMutationState state,
      ElementListType tree_type) = 0;

  virtual void OnCustomPropertyMutated(
      ElementId element_id,
      const std::string& custom_property_name,
      PaintWorkletInput::PropertyValue custom_property_value) = 0;
};

}  // namespace cc

#endif  // CC_TREES_MUTATOR_HOST_CLIENT_H_

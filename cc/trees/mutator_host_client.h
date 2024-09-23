// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MUTATOR_HOST_CLIENT_H_
#define CC_TREES_MUTATOR_HOST_CLIENT_H_

#include <optional>

#include "cc/base/protected_sequence_synchronizer.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/trees/property_animation_state.h"
#include "cc/trees/target_property.h"

namespace gfx {
class Transform;
class PointF;
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

class MutatorHostClient : public ProtectedSequenceSynchronizer {
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
      const gfx::PointF& scroll_offset) = 0;

  // Allows to change IsAnimating value for a set of properties.
  virtual void ElementIsAnimatingChanged(
      const PropertyToElementIdMap& element_id_map,
      ElementListType list_type,
      const PropertyAnimationState& mask,
      const PropertyAnimationState& state) = 0;

  virtual void MaximumScaleChanged(ElementId element_id,
                                   ElementListType list_type,
                                   float maximum_scale) = 0;

  virtual void ScrollOffsetAnimationFinished() = 0;

  virtual void NotifyAnimationWorkletStateChange(
      AnimationWorkletMutationState state,
      ElementListType tree_type) = 0;

  virtual void OnCustomPropertyMutated(
      PaintWorkletInput::PropertyKey property_key,
      PaintWorkletInput::PropertyValue property_value) = 0;

  virtual bool RunsOnCurrentThread() const = 0;
};

}  // namespace cc

#endif  // CC_TREES_MUTATOR_HOST_CLIENT_H_

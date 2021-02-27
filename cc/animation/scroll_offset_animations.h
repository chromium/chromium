// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_H_

#include <unordered_map>

#include "cc/animation/scroll_offset_animations_impl.h"
#include "cc/trees/mutator_host_client.h"

namespace cc {

// A ScrollOffsetAnimationUpdate represents a change on the main thread
// that may impact an impl-only scroll offset animation.
// Note that this class only exists on the main thread.
struct CC_ANIMATION_EXPORT ScrollOffsetAnimationUpdate {
  ScrollOffsetAnimationUpdate();
  explicit ScrollOffsetAnimationUpdate(ElementId);

  // The element to which this update applies.
  ElementId element_id_;

  // The amount by which the scroll offset is changed on the main thread.
  gfx::Vector2dF adjustment_;

  // If set to true, abort the currently running impl only scroll offset
  // animation running on cc and finish it on the main thread.
  bool takeover_;
};

// ScrollOffsetAnimations contains a list of ScrollOffsetAnimationUpdates.
// PushPropertiesTo is called during commit time and the necessary update is
// made to the impl-only animation.
class CC_ANIMATION_EXPORT ScrollOffsetAnimations {
 public:
  explicit ScrollOffsetAnimations(AnimationHost* animation_host);
  ~ScrollOffsetAnimations();

  void AddAdjustmentUpdate(ElementId, gfx::Vector2dF adjustment);
  void AddTakeoverUpdate(ElementId);

  bool HasUpdatesForTesting() const;

  // Goes through the updates in the order in which they're added and calls the
  // appropriate handler for each update.
  void PushPropertiesTo(ScrollOffsetAnimationsImpl*);

 private:
  ScrollOffsetAnimationUpdate GetUpdateForElementId(ElementId) const;

  using ElementToUpdateMap =
      std::unordered_map<ElementId, ScrollOffsetAnimationUpdate, ElementIdHash>;
  ElementToUpdateMap element_to_update_map_;

  AnimationHost* animation_host_;
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_MOCK_MUTATOR_HOST_H_
#define CC_TEST_MOCK_MUTATOR_HOST_H_

#include <memory>

#include "cc/trees/mutator_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cc {

class MockMutatorHost : public MutatorHost {
 public:
  MockMutatorHost();
  ~MockMutatorHost();

  MOCK_CONST_METHOD0(CreateImplInstance, std::unique_ptr<MutatorHost>());
  MOCK_METHOD0(ClearMutators, void());
  MOCK_METHOD1(UpdateRegisteredElementIds, void(ElementListType changed_list));
  MOCK_METHOD0(InitClientAnimationState, void());
  MOCK_METHOD2(RegisterElementId, void(ElementId, ElementListType));
  MOCK_METHOD2(UnregisterElementId,
               void(ElementId element_id, ElementListType list_type));
  MOCK_METHOD1(SetMutatorHostClient, void(MutatorHostClient* client));
  MOCK_METHOD1(SetLayerTreeMutator,
               void(std::unique_ptr<LayerTreeMutator> mutator));
  MOCK_METHOD1(PushPropertiesTo, void(MutatorHost* host_impl));
  MOCK_METHOD1(SetScrollAnimationDurationForTesting,
               void(base::TimeDelta duration));
  MOCK_CONST_METHOD0(NeedsTickAnimations, bool());
  MOCK_METHOD1(ActivateAnimations, bool(MutatorEvents* events));
  MOCK_METHOD3(TickAnimations,
               bool(base::TimeTicks monotonic_time,
                    const ScrollTree& scroll_tree,
                    bool is_active_tree));
  MOCK_METHOD2(TickScrollAnimations,
               void(base::TimeTicks monotonic_time,
                    const ScrollTree& scroll_tree));
  MOCK_METHOD0(TickWorkletAnimations, void());
  MOCK_METHOD2(UpdateAnimationState,
               bool(bool start_ready_animations, MutatorEvents* events));
  MOCK_METHOD1(TakeTimeUpdatedEvents, void(MutatorEvents* events));
  MOCK_METHOD0(PromoteScrollTimelinesPendingToActive, void());
  MOCK_METHOD0(CreateEvents, std::unique_ptr<MutatorEvents>());
  MOCK_METHOD1(SetAnimationEvents, void(std::unique_ptr<MutatorEvents> events));
  MOCK_CONST_METHOD1(ScrollOffsetAnimationWasInterrupted,
                     bool(ElementId element_id));
  MOCK_CONST_METHOD2(IsAnimatingFilterProperty,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(IsAnimatingBackdropFilterProperty,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(IsAnimatingOpacityProperty,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(IsAnimatingTransformProperty,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(HasPotentiallyRunningFilterAnimation,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(HasPotentiallyRunningBackdropFilterAnimation,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(HasPotentiallyRunningOpacityAnimation,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(HasPotentiallyRunningTransformAnimation,
                     bool(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD2(HasAnyAnimationTargetingProperty,
                     bool(ElementId element_id, TargetProperty::Type property));
  MOCK_CONST_METHOD1(AnimationsPreserveAxisAlignment,
                     bool(ElementId element_id));
  MOCK_CONST_METHOD2(MaximumScale,
                     float(ElementId element_id, ElementListType list_type));
  MOCK_CONST_METHOD1(IsElementAnimating, bool(ElementId element_id));
  MOCK_CONST_METHOD1(HasTickingKeyframeModelForTesting,
                     bool(ElementId element_id));
  MOCK_METHOD5(ImplOnlyAutoScrollAnimationCreate,
               void(ElementId element_id,
                    const gfx::ScrollOffset& target_offset,
                    const gfx::ScrollOffset& current_offset,
                    float autoscroll_velocity,
                    base::TimeDelta animation_start_offset));
  MOCK_METHOD5(ImplOnlyScrollAnimationCreate,
               void(ElementId element_id,
                    const gfx::ScrollOffset& target_offset,
                    const gfx::ScrollOffset& current_offset,
                    base::TimeDelta delayed_by,
                    base::TimeDelta animation_start_offset));
  MOCK_METHOD4(ImplOnlyScrollAnimationUpdateTarget,
               bool(const gfx::Vector2dF& scroll_delta,
                    const gfx::ScrollOffset& max_scroll_offset,
                    base::TimeTicks frame_monotonic_time,
                    base::TimeDelta delayed_by));
  MOCK_METHOD0(ScrollAnimationAbort, void());
  MOCK_CONST_METHOD0(ImplOnlyScrollAnimatingElement, ElementId());
  MOCK_CONST_METHOD0(MainThreadAnimationsCount, size_t());
  MOCK_CONST_METHOD0(HasCustomPropertyAnimations, bool());
  MOCK_CONST_METHOD0(CurrentFrameHadRAF, bool());
  MOCK_CONST_METHOD0(NextFrameHasPendingRAF, bool());
  MOCK_METHOD0(TakePendingThroughputTrackerInfos,
               PendingThroughputTrackerInfos());
  MOCK_CONST_METHOD0(HasCanvasInvalidation, bool());
  MOCK_CONST_METHOD0(HasJSAnimation, bool());
  MOCK_CONST_METHOD0(MinimumTickInterval, base::TimeDelta());
};

}  // namespace cc

#endif  // CC_TEST_MOCK_MUTATOR_HOST_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TIMELINES_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TIMELINES_TEST_COMMON_H_

#include <memory>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/keyframe/target_property.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {

class Animation;
class KeyframeEffect;

class TestLayer {
 public:
  static std::unique_ptr<TestLayer> Create();
  ~TestLayer();

  void ClearMutatedProperties();

  int transform_x() const;
  int transform_y() const;
  float brightness() const;
  float invert() const;

  const gfx::Transform& transform() const { return transform_; }
  void set_transform(const gfx::Transform& transform) {
    transform_ = transform;
    mutated_properties_[TargetProperty::TRANSFORM] = true;
  }

  float opacity() const { return opacity_; }
  void set_opacity(float opacity) {
    opacity_ = opacity;
    mutated_properties_[TargetProperty::OPACITY] = true;
  }

  FilterOperations filters() const { return filters_; }
  void set_filters(const FilterOperations& filters) {
    filters_ = filters;
    mutated_properties_[TargetProperty::FILTER] = true;
  }

  FilterOperations backdrop_filters() const { return backdrop_filters_; }
  void set_backdrop_filters(const FilterOperations& backdrop_filters) {
    backdrop_filters_ = backdrop_filters;
    mutated_properties_[TargetProperty::BACKDROP_FILTER] = true;
  }

  gfx::PointF scroll_offset() const { return scroll_offset_; }
  void set_scroll_offset(const gfx::PointF& scroll_offset) {
    scroll_offset_ = scroll_offset;
    mutated_properties_[TargetProperty::SCROLL_OFFSET] = true;
  }

  bool is_currently_animating(TargetProperty::Type property) const {
    return is_currently_animating_[property];
  }
  void set_is_currently_animating(TargetProperty::Type property,
                                  bool is_animating) {
    is_currently_animating_[property] = is_animating;
  }

  bool has_potential_animation(TargetProperty::Type property) const {
    return has_potential_animation_[property];
  }
  void set_has_potential_animation(TargetProperty::Type property,
                                   bool is_animating) {
    has_potential_animation_[property] = is_animating;
  }

  bool is_property_mutated(TargetProperty::Type property) const {
    return mutated_properties_[property];
  }

  float maximum_animation_scale() const { return maximum_animation_scale_; }
  void set_maximum_animation_scale(float scale) {
    maximum_animation_scale_ = scale;
  }

 private:
  TestLayer();

  gfx::Transform transform_;
  float opacity_;
  FilterOperations filters_;
  FilterOperations backdrop_filters_;
  gfx::PointF scroll_offset_;

  gfx::TargetProperties has_potential_animation_;
  gfx::TargetProperties is_currently_animating_;
  gfx::TargetProperties mutated_properties_;
  float maximum_animation_scale_ = kInvalidScale;
};

class TestHostClient : public MutatorHostClient {
 public:
  explicit TestHostClient(ThreadInstance thread_instance);
  ~TestHostClient() override;

  void ClearMutatedProperties();

  bool IsElementInPropertyTrees(ElementId element_id,
                                ElementListType list_type) const override;

  void SetMutatorsNeedCommit() override;
  void SetMutatorsNeedRebuildPropertyTrees() override;

  void SetElementFilterMutated(ElementId element_id,
                               ElementListType list_type,
                               const FilterOperations& filters) override;

  void SetElementBackdropFilterMutated(
      ElementId element_id,
      ElementListType list_type,
      const FilterOperations& backdrop_filters) override;

  void SetElementOpacityMutated(ElementId element_id,
                                ElementListType list_type,
                                float opacity) override;

  void SetElementTransformMutated(ElementId element_id,
                                  ElementListType list_type,
                                  const gfx::Transform& transform) override;

  void SetElementScrollOffsetMutated(ElementId element_id,
                                     ElementListType list_type,
                                     const gfx::PointF& scroll_offset) override;

  void ElementIsAnimatingChanged(const PropertyToElementIdMap& element_id_map,
                                 ElementListType list_type,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) override;
  void MaximumScaleChanged(ElementId element_id,
                           ElementListType list_type,
                           float maximum_scale) override;

  void ScrollOffsetAnimationFinished() override {}

  void SetScrollOffsetForAnimation(const gfx::PointF& scroll_offset,
                                   ElementId element_id);
  const PropertyTrees& GetPropertyTrees() const { return property_trees_; }

  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override {}

  void OnCustomPropertyMutated(
      PaintWorkletInput::PropertyKey property_key,
      PaintWorkletInput::PropertyValue property_value) override {}

  bool RunsOnCurrentThread() const override;

  bool IsOwnerThread() const override;
  bool InProtectedSequence() const override;
  void WaitForProtectedSequenceCompletion() const override;

  bool mutators_need_commit() const { return mutators_need_commit_; }
  void set_mutators_need_commit(bool need) { mutators_need_commit_ = need; }

  void RegisterElementId(ElementId element_id, ElementListType list_type);
  void UnregisterElementId(ElementId element_id, ElementListType list_type);

  AnimationHost* host() {
    DCHECK(host_);
    return host_.get();
  }

  bool IsPropertyMutated(ElementId element_id,
                         ElementListType list_type,
                         TargetProperty::Type property) const;

  FilterOperations GetFilters(ElementId element_id,
                              ElementListType list_type) const;
  FilterOperations GetBackdropFilters(ElementId element_id,
                                      ElementListType list_type) const;
  float GetOpacity(ElementId element_id, ElementListType list_type) const;
  gfx::Transform GetTransform(ElementId element_id,
                              ElementListType list_type) const;
  gfx::PointF GetScrollOffset(ElementId element_id,
                              ElementListType list_type) const;
  bool GetHasPotentialTransformAnimation(ElementId element_id,
                                         ElementListType list_type) const;
  bool GetTransformIsCurrentlyAnimating(ElementId element_id,
                                        ElementListType list_type) const;
  bool GetOpacityIsCurrentlyAnimating(ElementId element_id,
                                      ElementListType list_type) const;
  bool GetHasPotentialOpacityAnimation(ElementId element_id,
                                       ElementListType list_type) const;
  bool GetHasPotentialFilterAnimation(ElementId element_id,
                                      ElementListType list_type) const;
  bool GetFilterIsCurrentlyAnimating(ElementId element_id,
                                     ElementListType list_type) const;
  bool GetHasPotentialBackdropFilterAnimation(ElementId element_id,
                                              ElementListType list_type) const;
  bool GetBackdropFilterIsCurrentlyAnimating(ElementId element_id,
                                             ElementListType list_type) const;

  void ExpectFilterPropertyMutated(ElementId element_id,
                                   ElementListType list_type,
                                   float brightness) const;
  void ExpectBackdropFilterPropertyMutated(ElementId element_id,
                                           ElementListType list_type,
                                           float invert) const;
  void ExpectOpacityPropertyMutated(ElementId element_id,
                                    ElementListType list_type,
                                    float opacity) const;
  void ExpectTransformPropertyMutated(ElementId element_id,
                                      ElementListType list_type,
                                      int transform_x,
                                      int transform_y) const;

  TestLayer* FindTestLayer(ElementId element_id,
                           ElementListType list_type) const;

 private:
  std::unique_ptr<AnimationHost> host_;

  using ElementIdToTestLayer =
      std::unordered_map<ElementId, std::unique_ptr<TestLayer>, ElementIdHash>;
  ElementIdToTestLayer layers_in_active_tree_;
  ElementIdToTestLayer layers_in_pending_tree_;

  bool mutators_need_commit_;
  PropertyTrees property_trees_;
};

class TestAnimationDelegate : public AnimationDelegate {
 public:
  TestAnimationDelegate();

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override;
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationTakeover(
      base::TimeTicks monotonic_time,
      int target_property,
      base::TimeTicks animation_start_time,
      std::unique_ptr<gfx::AnimationCurve> curve) override;
  void NotifyLocalTimeUpdated(
      std::optional<base::TimeDelta> local_time) override;

  bool started() { return started_; }

  bool finished() { return finished_; }

  bool aborted() { return aborted_; }

  bool takeover() { return takeover_; }

  base::TimeTicks start_time() { return start_time_; }

 private:
  bool started_;
  bool finished_;
  bool aborted_;
  bool takeover_;
  base::TimeTicks start_time_;
};

class AnimationTimelinesTest : public testing::Test {
 public:
  AnimationTimelinesTest();
  ~AnimationTimelinesTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  void GetImplTimelineAndAnimationByID();

  void CreateTestLayer(bool needs_active_value_observations,
                       bool needs_pending_value_observations);
  void AttachTimelineAnimationLayer();
  virtual void CreateImplTimelineAndAnimation();

  void CreateTestMainLayer();
  void DestroyTestMainLayer();
  void CreateTestImplLayer(ElementListType element_list_type);

  void ReleaseRefPtrs();

  void TickAnimationsTransferEvents(base::TimeTicks time,
                                    unsigned expect_events);

  KeyframeEffect* GetKeyframeEffectForElementId(ElementId element_id);
  KeyframeEffect* GetImplKeyframeEffectForLayerId(ElementId element_id);

  bool CheckKeyframeEffectTimelineNeedsPushProperties(
      bool needs_push_properties) const;

  void PushProperties();

  TestHostClient client_;
  TestHostClient client_impl_;

  raw_ptr<AnimationHost> host_;
  raw_ptr<AnimationHost> host_impl_;

  const int timeline_id_;
  const int animation_id_;
  ElementId element_id_;

  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<Animation> animation_;
  scoped_refptr<const ElementAnimations> element_animations_;

  scoped_refptr<AnimationTimeline> timeline_impl_;
  scoped_refptr<Animation> animation_impl_;
  scoped_refptr<const ElementAnimations> element_animations_impl_;
};

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TIMELINES_TEST_COMMON_H_

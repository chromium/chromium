// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_WORKLET_ANIMATION_H_
#define CC_ANIMATION_WORKLET_ANIMATION_H_

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/single_keyframe_effect_animation.h"
#include "cc/trees/property_tree.h"

namespace cc {

namespace {
FORWARD_DECLARE_TEST(WorkletAnimationTest, NonImplInstanceDoesNotTickKeyframe);
}  // namespace

class AnimationOptions;
class ScrollTimeline;

// A WorkletAnimation is an animation that allows its animation
// timing to be controlled by an animator instance that is running in a
// AnimationWorkletGlobalScope.
class CC_ANIMATION_EXPORT WorkletAnimation final
    : public SingleKeyframeEffectAnimation {
 public:
  enum class State { PENDING, RUNNING, REMOVED };
  WorkletAnimation(int cc_animation_id,
                   WorkletAnimationId worklet_animation_id,
                   const std::string& name,
                   std::unique_ptr<ScrollTimeline> scroll_timeline,
                   std::unique_ptr<AnimationOptions> options,
                   bool is_controlling_instance);
  static scoped_refptr<WorkletAnimation> Create(
      WorkletAnimationId worklet_animation_id,
      const std::string& name,
      std::unique_ptr<ScrollTimeline> scroll_timeline,
      std::unique_ptr<AnimationOptions> options);
  scoped_refptr<Animation> CreateImplInstance() const override;

  WorkletAnimationId worklet_animation_id() { return worklet_animation_id_; }
  const std::string& name() const { return name_; }
  const ScrollTimeline* scroll_timeline() const {
    return scroll_timeline_.get();
  }

  bool IsWorkletAnimation() const override;

  void Tick(base::TimeTicks monotonic_time) override;

  void UpdateInputState(MutatorInputState* input_state,
                        base::TimeTicks monotonic_time,
                        const ScrollTree& scroll_tree,
                        bool is_active_tree);
  void SetOutputState(const MutatorOutputState::AnimationState& state);

  void PushPropertiesTo(Animation* animation_impl) override;

  // Should be called when the ScrollTimeline attached to this animation has a
  // change, such as when the scroll source changes ElementId.
  void UpdateScrollTimeline(base::Optional<ElementId> scroller_id,
                            base::Optional<double> start_scroll_offset,
                            base::Optional<double> end_scroll_offset);

  // Should be called when the pending tree is promoted to active, as this may
  // require updating the ElementId for the ScrollTimeline scroll source.
  void PromoteScrollTimelinePendingToActive() override;

  void RemoveKeyframeModel(int keyframe_model_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkletAnimationTest,
                           NonImplInstanceDoesNotTickKeyframe);
  WorkletAnimation(int cc_animation_id,
                   WorkletAnimationId worklet_animation_id,
                   const std::string& name,
                   std::unique_ptr<ScrollTimeline> scroll_timeline,
                   std::unique_ptr<AnimationOptions> options,
                   bool is_controlling_instance,
                   std::unique_ptr<KeyframeEffect> effect);

  ~WorkletAnimation() override;

  // Returns the current time to be passed into the underlying AnimationWorklet.
  // The current time is based on the timeline associated with the animation.
  double CurrentTime(base::TimeTicks monotonic_time,
                     const ScrollTree& scroll_tree,
                     bool is_active_tree);

  // Returns true if the worklet animation needs to be updated which happens iff
  // its current time is going to be different from last time given these input.
  bool NeedsUpdate(base::TimeTicks monotonic_time,
                   const ScrollTree& scroll_tree,
                   bool is_active_tree);

  std::unique_ptr<AnimationOptions> CloneOptions() const {
    return options_ ? options_->Clone() : nullptr;
  }

  WorkletAnimationId worklet_animation_id_;
  std::string name_;

  // The ScrollTimeline associated with the underlying animation. If null, the
  // animation is based on a DocumentTimeline.
  //
  // TODO(crbug.com/780148): A WorkletAnimation should own an AnimationTimeline
  // which must exist but can either be a DocumentTimeline, ScrollTimeline, or
  // some other future implementation.
  std::unique_ptr<ScrollTimeline> scroll_timeline_;

  std::unique_ptr<AnimationOptions> options_;

  // Local time is used as an input to the keyframe effect of this animation.
  // The value comes from the user script that runs inside the animation worklet
  // global scope.
  base::Optional<base::TimeDelta> local_time_;

  base::Optional<base::TimeTicks> start_time_;
  // Last current time used for updatig. We use this to skip updating if current
  // time has not changed since last update.
  base::Optional<double> last_current_time_;

  State state_;

  bool is_impl_instance_;
};

inline WorkletAnimation* ToWorkletAnimation(Animation* animation) {
  DCHECK(animation->IsWorkletAnimation());
  return static_cast<WorkletAnimation*>(animation);
}

}  // namespace cc

#endif  // CC_ANIMATION_WORKLET_ANIMATION_H_

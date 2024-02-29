// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_WORKLET_ANIMATION_H_
#define CC_ANIMATION_WORKLET_ANIMATION_H_

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_host.h"
#include "cc/trees/property_tree.h"

namespace cc {

class AnimationOptions;
class AnimationEffectTimings;

// A WorkletAnimation is an animation that allows its animation
// timing to be controlled by an animator instance that is running in a
// AnimationWorkletGlobalScope.

// Two instances of this class are created for Blink WorkletAnimation:
// 1. UI thread instance that keeps all the meta data.
// 2. Impl thread instance that ticks the animations on the Impl thread.
// When Blink WorkletAnimation is updated, it calls the UI thread instance to
// modify its properties. The updated properties are pushed by the UI thread
// instance to the Impl thread instance during commit.
class CC_ANIMATION_EXPORT WorkletAnimation final : public Animation {
 public:
  enum class State { kPending, kRunning, kRemoved };
  WorkletAnimation(int cc_animation_id,
                   WorkletAnimationId worklet_animation_id,
                   const std::string& name,
                   double playback_rate_value,
                   std::unique_ptr<AnimationOptions> options,
                   std::unique_ptr<AnimationEffectTimings> effect_timings,
                   bool is_controlling_instance);
  static scoped_refptr<WorkletAnimation> Create(
      WorkletAnimationId worklet_animation_id,
      const std::string& name,
      double playback_rate_value,
      std::unique_ptr<AnimationOptions> options,
      std::unique_ptr<AnimationEffectTimings> effect_timings);
  scoped_refptr<Animation> CreateImplInstance() const override;

  WorkletAnimationId worklet_animation_id() { return worklet_animation_id_; }
  const std::string& name() const { return name_; }

  bool IsWorkletAnimation() const override;

  bool Tick(base::TimeTicks monotonic_time) override;

  void UpdateState(bool start_ready_animations,
                   AnimationEvents* events) override;

  void TakeTimeUpdatedEvent(AnimationEvents* events) override;
  void UpdateInputState(MutatorInputState* input_state,
                        base::TimeTicks monotonic_time,
                        const ScrollTree& scroll_tree,
                        bool is_active_tree);
  void SetOutputState(const MutatorOutputState::AnimationState& state);

  void PushPropertiesTo(Animation* animation_impl) override;

  // Called by Blink WorkletAnimation when its playback rate is updated.
  void UpdatePlaybackRate(double rate);
  void SetPlaybackRateForTesting(double playback_rate) {
    SetPlaybackRate(playback_rate);
  }

  void RemoveKeyframeModel(int keyframe_model_id) override;
  void ReleasePendingTreeLock();

 private:
  ~WorkletAnimation() override;

  double playback_rate() const { return playback_rate_.Read(*this); }

  // Returns the current time to be passed into the underlying AnimationWorklet.
  // The current time is based on the timeline associated with the animation and
  // in case of scroll timeline it may be nullopt when the associated scrolling
  // node is not available in the particular ScrollTree.
  std::optional<base::TimeDelta> CurrentTime(base::TimeTicks monotonic_time,
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

  std::unique_ptr<AnimationEffectTimings> CloneEffectTimings() const {
    return effect_timings_ ? effect_timings_->Clone() : nullptr;
  }

  // Updates the playback rate of the Impl thread instance.
  // Called by the UI thread WorkletAnimation instance during commit.
  void SetPlaybackRate(double rate);

  bool IsTimelineActive(const ScrollTree& scroll_tree,
                        bool is_active_tree) const;

  const WorkletAnimationId worklet_animation_id_;
  const std::string name_;

  // Controls speed of the animation.
  // https://drafts.csswg.org/web-animations-2/#animation-effect-playback-rate

  // For UI thread instance contains the meta value to be pushed to the Impl
  // thread instance.
  // For the Impl thread instance contains the actual playback rate of the
  // animation.
  ProtectedSequenceReadable<double> playback_rate_;

  // These are set once, and never change.
  std::unique_ptr<AnimationOptions> options_;
  std::unique_ptr<AnimationEffectTimings> effect_timings_;

  // Local time is used as an input to the keyframe effect of this animation.
  // The value comes from the user script that runs inside the animation worklet
  // global scope.
  ProtectedSequenceReadable<std::optional<base::TimeDelta>> local_time_;
  // Local time passed to the main thread worklet animation to update its
  // keyframe effect. We only set the most recent local time, meaning that if
  // there are multiple compositor frames without a single main frame only
  // the local time associated with the latest frame is sent to the main thread.
  ProtectedSequenceReadable<std::optional<base::TimeDelta>>
      last_synced_local_time_;

  ProtectedSequenceReadable<std::optional<base::TimeTicks>> start_time_;

  // Last current time used for updating. We use this to skip updating if
  // current time has not changed since last update.
  ProtectedSequenceReadable<std::optional<base::TimeDelta>> last_current_time_;

  // To ensure that 'time' progresses forward for scroll animations, we guard
  // against allowing active tree mutations while the pending tree has a
  // lock in the worklet. The lock is established when updating the input state
  // for the pending tree and release on pending tree activation.
  ProtectedSequenceReadable<bool> has_pending_tree_lock_{false};
  ProtectedSequenceReadable<State> state_{State::kPending};

  const bool is_impl_instance_;
};

inline WorkletAnimation* ToWorkletAnimation(Animation* animation) {
  DCHECK(animation->IsWorkletAnimation());
  return static_cast<WorkletAnimation*>(animation);
}

}  // namespace cc

#endif  // CC_ANIMATION_WORKLET_ANIMATION_H_

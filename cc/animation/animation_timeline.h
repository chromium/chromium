// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_TIMELINE_H_
#define CC_ANIMATION_ANIMATION_TIMELINE_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/animation/animation_export.h"
#include "cc/base/protected_sequence_synchronizer.h"
#include "cc/paint/element_id.h"

namespace base {
class TimeTicks;
}

namespace cc {

class Animation;
class AnimationHost;
class ScrollTree;

// An AnimationTimeline owns a group of Animations.
//
// Each AnimationTimeline and its Animations have copies on the impl thread. We
// synchronize the main and impl thread instances using their IDs.
class CC_ANIMATION_EXPORT AnimationTimeline
    : public base::RefCounted<AnimationTimeline>,
      public ProtectedSequenceSynchronizer {
 public:
  static scoped_refptr<AnimationTimeline> Create(int id,
                                                 bool impl_only = false);
  virtual scoped_refptr<AnimationTimeline> CreateImplInstance() const;

  AnimationTimeline(const AnimationTimeline&) = delete;
  AnimationTimeline& operator=(const AnimationTimeline&) = delete;

  int id() const { return id_; }

  using IdToAnimationMap = std::unordered_map<int, scoped_refptr<Animation>>;
  const IdToAnimationMap& animations() const {
    return id_to_animation_map_.Read(*this);
  }

  // Parent AnimationHost.
  AnimationHost* animation_host() {
    DCHECK(IsOwnerThread() || InProtectedSequence());
    return animation_host_;
  }
  const AnimationHost* animation_host() const {
    DCHECK(IsOwnerThread() || InProtectedSequence());
    return animation_host_;
  }
  void SetAnimationHost(AnimationHost* animation_host);

  bool is_impl_only() const { return is_impl_only_; }

  void AttachAnimation(scoped_refptr<Animation> animation);
  void DetachAnimation(scoped_refptr<Animation> animation);

  void ClearAnimations();
  bool HasAnimation() const {
    return !id_to_animation_map_.Read(*this).empty();
  }
  bool TickTimeLinkedAnimations(
      const std::vector<scoped_refptr<Animation>>& ticking_animations,
      base::TimeTicks monotonic_time,
      bool tick_finished);
  virtual bool TickScrollLinkedAnimations(
      const std::vector<scoped_refptr<Animation>>& ticking_animations,
      const ScrollTree& scroll_tree,
      bool is_active_tree);

  virtual void PushPropertiesTo(AnimationTimeline* timeline_impl);
  virtual void ActivateTimeline() {}

  Animation* GetAnimationById(int animation_id) const;

  void SetNeedsPushProperties();
  bool needs_push_properties() const {
    return needs_push_properties_.Read(*this);
  }

  virtual bool IsScrollTimeline() const;
  virtual bool IsLinkedToScroller(ElementId scroller) const;

  // ProtectedSequenceSynchronizer implementation
  bool IsOwnerThread() const override;
  bool InProtectedSequence() const override;
  void WaitForProtectedSequenceCompletion() const override;

 protected:
  AnimationTimeline(int id, bool impl_only);
  ~AnimationTimeline() override;

  // A list of all animations which this timeline owns.
  ProtectedSequenceWritable<IdToAnimationMap> id_to_animation_map_;

 private:
  friend class base::RefCounted<AnimationTimeline>;

  void PushAttachedAnimationsToImplThread(AnimationTimeline* timeline) const;
  void RemoveDetachedAnimationsFromImplThread(
      AnimationTimeline* timeline) const;
  void PushPropertiesToImplThread(AnimationTimeline* timeline);

  void EraseAnimation(scoped_refptr<Animation> animation);

  const int id_;
  raw_ptr<AnimationHost> animation_host_;
  ProtectedSequenceWritable<bool> needs_push_properties_;

  // Impl-only AnimationTimeline has no main thread instance and lives on
  // it's own.
  const bool is_impl_only_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TIMELINE_H_

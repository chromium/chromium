// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_TIMELINE_H_
#define CC_ANIMATION_ANIMATION_TIMELINE_H_

#include <memory>
#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "cc/animation/animation_export.h"

namespace cc {

class Animation;
class AnimationHost;
class ScrollTree;

// An AnimationTimeline owns a group of Animations.
//
// Each AnimationTimeline and its Animations have copies on the impl thread. We
// synchronize the main and impl thread instances using their IDs.
class CC_ANIMATION_EXPORT AnimationTimeline
    : public base::RefCounted<AnimationTimeline> {
 public:
  static scoped_refptr<AnimationTimeline> Create(int id);
  virtual scoped_refptr<AnimationTimeline> CreateImplInstance() const;

  AnimationTimeline(const AnimationTimeline&) = delete;
  AnimationTimeline& operator=(const AnimationTimeline&) = delete;

  int id() const { return id_; }

  // Parent AnimationHost.
  AnimationHost* animation_host() { return animation_host_; }
  const AnimationHost* animation_host() const { return animation_host_; }
  void SetAnimationHost(AnimationHost* animation_host);

  void set_is_impl_only(bool is_impl_only) { is_impl_only_ = is_impl_only; }
  bool is_impl_only() const { return is_impl_only_; }

  void AttachAnimation(scoped_refptr<Animation> animation);
  void DetachAnimation(scoped_refptr<Animation> animation);

  void ClearAnimations();
  bool HasAnimation() const { return !id_to_animation_map_.empty(); }
  bool TickTimeLinkedAnimations(
      const std::vector<scoped_refptr<Animation>>& ticking_animations,
      base::TimeTicks monotonic_time);
  virtual bool TickScrollLinkedAnimations(
      const std::vector<scoped_refptr<Animation>>& ticking_animations,
      const ScrollTree& scroll_tree,
      bool is_active_tree);

  virtual void PushPropertiesTo(AnimationTimeline* timeline_impl);
  virtual void ActivateTimeline() {}

  Animation* GetAnimationById(int animation_id) const;

  void SetNeedsPushProperties();
  bool needs_push_properties() const { return needs_push_properties_; }

  virtual bool IsScrollTimeline() const;

 protected:
  explicit AnimationTimeline(int id);
  virtual ~AnimationTimeline();

  // A list of all animations which this timeline owns.
  using IdToAnimationMap = std::unordered_map<int, scoped_refptr<Animation>>;
  IdToAnimationMap id_to_animation_map_;

 private:
  friend class base::RefCounted<AnimationTimeline>;

  void PushAttachedAnimationsToImplThread(AnimationTimeline* timeline) const;
  void RemoveDetachedAnimationsFromImplThread(
      AnimationTimeline* timeline) const;
  void PushPropertiesToImplThread(AnimationTimeline* timeline);

  void EraseAnimation(scoped_refptr<Animation> animation);

  int id_;
  AnimationHost* animation_host_;
  bool needs_push_properties_;

  // Impl-only AnimationTimeline has no main thread instance and lives on
  // it's own.
  bool is_impl_only_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TIMELINE_H_

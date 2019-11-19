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

// An AnimationTimeline owns a group of Animations.
//
// Each AnimationTimeline and its Animations have copies on the impl thread. We
// synchronize the main and impl thread instances using their IDs.
class CC_ANIMATION_EXPORT AnimationTimeline
    : public base::RefCounted<AnimationTimeline> {
 public:
  static scoped_refptr<AnimationTimeline> Create(int id);
  scoped_refptr<AnimationTimeline> CreateImplInstance() const;

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

  void PushPropertiesTo(AnimationTimeline* timeline_impl);

  Animation* GetAnimationById(int animation_id) const;

  void SetNeedsPushProperties();
  bool needs_push_properties() const { return needs_push_properties_; }

 private:
  friend class base::RefCounted<AnimationTimeline>;

  explicit AnimationTimeline(int id);
  virtual ~AnimationTimeline();

  void PushAttachedAnimationsToImplThread(AnimationTimeline* timeline) const;
  void RemoveDetachedAnimationsFromImplThread(
      AnimationTimeline* timeline) const;
  void PushPropertiesToImplThread(AnimationTimeline* timeline);

  void EraseAnimation(scoped_refptr<Animation> animation);

  // A list of all animations which this timeline owns.
  using IdToAnimationMap = std::unordered_map<int, scoped_refptr<Animation>>;
  IdToAnimationMap id_to_animation_map_;

  int id_;
  AnimationHost* animation_host_;
  bool needs_push_properties_;

  // Impl-only AnimationTimeline has no main thread instance and lives on
  // it's own.
  bool is_impl_only_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TIMELINE_H_

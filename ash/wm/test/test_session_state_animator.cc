// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/test/test_session_state_animator.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"

namespace ash {

const SessionStateAnimator::Container
    TestSessionStateAnimator::kAllContainers[] = {
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::SHELF,
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::LOCK_SCREEN_WALLPAPER,
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::LOCK_SCREEN_RELATED_CONTAINERS,
        SessionStateAnimator::ROOT_CONTAINER};

// A simple SessionStateAnimator::AnimationSequence that tracks the number of
// attached sequences.  The callback will be invoked if all animations complete
// successfully.
class TestSessionStateAnimator::AnimationSequence
    : public SessionStateAnimator::AnimationSequence {
 public:
  AnimationSequence(AnimationCallback callback,
                    TestSessionStateAnimator* animator)
      : SessionStateAnimator::AnimationSequence(std::move(callback)),
        animator_(animator) {}

  AnimationSequence(const AnimationSequence&) = delete;
  AnimationSequence& operator=(const AnimationSequence&) = delete;

  ~AnimationSequence() override = default;

  virtual void SequenceAttached() { ++sequence_count_; }

  // Notify the sequence that is has completed.
  virtual void SequenceFinished(bool successfully) {
    DCHECK_GT(sequence_count_, 0);
    --sequence_count_;
    sequence_aborted_ |= !successfully;
    if (sequence_count_ == 0) {
      if (sequence_aborted_)
        OnAnimationAborted();
      else
        OnAnimationCompleted();
    }
  }

  // ash::SessionStateAnimator::AnimationSequence:
  void StartAnimation(int container_mask,
                      AnimationType type,
                      AnimationSpeed speed) override {
    animator_->StartAnimationInSequence(container_mask, type, speed, this);
  }

 private:
  // Tracks the number of contained animations.
  int sequence_count_ = 0;

  // True if the sequence was aborted.
  bool sequence_aborted_ = false;

  // The TestSessionAnimator that created this.  Not owned.
  raw_ptr<TestSessionStateAnimator> animator_;
};

TestSessionStateAnimator::ActiveAnimation::ActiveAnimation(
    int animation_epoch,
    base::TimeDelta duration,
    SessionStateAnimator::Container container,
    AnimationType type,
    AnimationSpeed speed,
    base::OnceClosure success_callback,
    base::OnceClosure failed_callback)
    : animation_epoch(animation_epoch),
      remaining_duration(duration),
      container(container),
      type(type),
      speed(speed),
      success_callback(std::move(success_callback)),
      failed_callback(std::move(failed_callback)) {}

TestSessionStateAnimator::ActiveAnimation::ActiveAnimation(
    ActiveAnimation&& other) = default;

TestSessionStateAnimator::ActiveAnimation&
TestSessionStateAnimator::ActiveAnimation::operator=(ActiveAnimation&& other) =
    default;

TestSessionStateAnimator::ActiveAnimation::~ActiveAnimation() = default;

TestSessionStateAnimator::TestSessionStateAnimator() = default;

TestSessionStateAnimator::~TestSessionStateAnimator() {
  CompleteAllAnimations(false);
}

void TestSessionStateAnimator::ResetAnimationEpoch() {
  CompleteAllAnimations(false);
  last_animation_epoch_ = 0;
}

void TestSessionStateAnimator::Advance(const base::TimeDelta& duration) {
  for (ActiveAnimationsMap::iterator container_iter =
           active_animations_.begin();
       container_iter != active_animations_.end(); ++container_iter) {
    AnimationList::iterator animation_iter = (*container_iter).second.begin();
    while (animation_iter != (*container_iter).second.end()) {
      ActiveAnimation& active_animation = *animation_iter;
      active_animation.remaining_duration -= duration;
      if (active_animation.remaining_duration <= base::TimeDelta()) {
        // Save callback and erase animation, then run the callback afterwards
        // to avoid running the callback twice for animations that start other
        // animations on their same container. This is because the second
        // animation will call the first's animation callback when being added
        // to the animations list by aborting it, as we don't support 2
        // animations on the same container in this object.
        auto success_callback = std::move(active_animation.success_callback);
        animation_iter = (*container_iter).second.erase(animation_iter);
        std::move(success_callback).Run();
      } else {
        ++animation_iter;
      }
    }
  }
}

void TestSessionStateAnimator::CompleteAnimations(int animation_epoch,
                                                  bool completed_successfully) {
  for (ActiveAnimationsMap::iterator container_iter =
           active_animations_.begin();
       container_iter != active_animations_.end(); ++container_iter) {
    AnimationList::iterator animation_iter = (*container_iter).second.begin();
    while (animation_iter != (*container_iter).second.end()) {
      ActiveAnimation& active_animation = *animation_iter;
      if (active_animation.animation_epoch <= animation_epoch) {
        if (completed_successfully)
          std::move(active_animation.success_callback).Run();
        else
          std::move(active_animation.failed_callback).Run();
        animation_iter = (*container_iter).second.erase(animation_iter);
      } else {
        ++animation_iter;
      }
    }
  }
}

void TestSessionStateAnimator::CompleteAllAnimations(
    bool completed_successfully) {
  CompleteAnimations(last_animation_epoch_, completed_successfully);
}

bool TestSessionStateAnimator::IsContainerAnimated(
    SessionStateAnimator::Container container,
    SessionStateAnimator::AnimationType type) const {
  ActiveAnimationsMap::const_iterator container_iter =
      active_animations_.find(container);
  if (container_iter != active_animations_.end()) {
    for (AnimationList::const_iterator animation_iter =
             (*container_iter).second.begin();
         animation_iter != (*container_iter).second.end(); ++animation_iter) {
      const ActiveAnimation& active_animation = *animation_iter;
      if (active_animation.type == type)
        return true;
    }
  }
  return false;
}

bool TestSessionStateAnimator::AreContainersAnimated(
    int container_mask,
    SessionStateAnimator::AnimationType type) const {
  for (size_t i = 0; i < std::size(kAllContainers); ++i) {
    if (container_mask & kAllContainers[i] &&
        !IsContainerAnimated(kAllContainers[i], type)) {
      return false;
    }
  }
  return true;
}

size_t TestSessionStateAnimator::GetAnimationCount() const {
  size_t count = 0;
  for (ActiveAnimationsMap::const_iterator container_iter =
           active_animations_.begin();
       container_iter != active_animations_.end(); ++container_iter) {
    count += (*container_iter).second.size();
  }
  return count;
}

void TestSessionStateAnimator::StartAnimation(int container_mask,
                                              AnimationType type,
                                              AnimationSpeed speed) {
  ++last_animation_epoch_;
  for (size_t i = 0; i < std::size(kAllContainers); ++i) {
    if (container_mask & kAllContainers[i]) {
      AddAnimation(kAllContainers[i], type, speed, base::DoNothing(),
                   base::DoNothing());
    }
  }
}

void TestSessionStateAnimator::StartAnimationWithCallback(
    int container_mask,
    AnimationType type,
    AnimationSpeed speed,
    base::OnceClosure callback) {
  ++last_animation_epoch_;

  int container_count = 0;
  for (size_t i = 0; i < std::size(kAllContainers); ++i) {
    if (container_mask & kAllContainers[i])
      ++container_count;
  }

  base::RepeatingClosure completion_callback =
      base::BarrierClosure(container_count, std::move(callback));
  for (size_t i = 0; i < std::size(kAllContainers); ++i) {
    if (container_mask & kAllContainers[i]) {
      // ash::SessionStateAnimatorImpl invokes the callback whether or not the
      // animation was completed successfully or not.
      AddAnimation(kAllContainers[i], type, speed, completion_callback,
                   completion_callback);
    }
  }
}

SessionStateAnimator::AnimationSequence*
TestSessionStateAnimator::BeginAnimationSequence(AnimationCallback callback) {
  return new AnimationSequence(std::move(callback), this);
}

bool TestSessionStateAnimator::IsWallpaperHidden() const {
  return is_wallpaper_hidden_;
}

void TestSessionStateAnimator::ShowWallpaper() {
  is_wallpaper_hidden_ = false;
}

void TestSessionStateAnimator::HideWallpaper() {
  is_wallpaper_hidden_ = true;
}

void TestSessionStateAnimator::AbortAnimations(int container_mask) {
  for (size_t i = 0; i < std::size(kAllContainers); ++i) {
    if (container_mask & kAllContainers[i])
      AbortAnimation(kAllContainers[i]);
  }
}

void TestSessionStateAnimator::StartAnimationInSequence(
    int container_mask,
    AnimationType type,
    AnimationSpeed speed,
    AnimationSequence* animation_sequence) {
  ++last_animation_epoch_;
  for (size_t i = 0; i < std::size(kAllContainers); ++i) {
    if (container_mask & kAllContainers[i]) {
      base::OnceClosure success_callback =
          base::BindOnce(&AnimationSequence::SequenceFinished,
                         base::Unretained(animation_sequence), true);
      base::OnceClosure failed_callback =
          base::BindOnce(&AnimationSequence::SequenceFinished,
                         base::Unretained(animation_sequence), false);
      animation_sequence->SequenceAttached();
      AddAnimation(kAllContainers[i], type, speed, std::move(success_callback),
                   std::move(failed_callback));
    }
  }
}

void TestSessionStateAnimator::AddAnimation(
    SessionStateAnimator::Container container,
    AnimationType type,
    AnimationSpeed speed,
    base::OnceClosure success_callback,
    base::OnceClosure failed_callback) {
  base::TimeDelta duration = GetDuration(speed);
  ActiveAnimation active_animation(last_animation_epoch_, duration, container,
                                   type, speed, std::move(success_callback),
                                   std::move(failed_callback));
  // This test double is limited to only have one animation active for a given
  // container at a time.
  AbortAnimation(container);
  active_animations_[container].push_back(std::move(active_animation));
}

void TestSessionStateAnimator::AbortAnimation(
    SessionStateAnimator::Container container) {
  ActiveAnimationsMap::iterator container_iter =
      active_animations_.find(container);
  if (container_iter != active_animations_.end()) {
    AnimationList::iterator animation_iter = (*container_iter).second.begin();
    while (animation_iter != (*container_iter).second.end()) {
      ActiveAnimation& active_animation = *animation_iter;
      std::move(active_animation.failed_callback).Run();
      animation_iter = (*container_iter).second.erase(animation_iter);
    }
  }
}

}  // namespace ash

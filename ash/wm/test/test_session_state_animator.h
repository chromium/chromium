// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_TEST_SESSION_STATE_ANIMATOR_H_
#define ASH_WM_TEST_TEST_SESSION_STATE_ANIMATOR_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "ash/wm/session_state_animator.h"
#include "base/time/time.h"

namespace ash {

// A SessionStateAnimator that offers control over the lifetime of active
// animations.
// NOTE: The TestSessionStateAnimator limits each
// SessionStateAnimator::Container to a single active animation at any one time.
// If a new animation is started on a container the existing one will be
// aborted.
class TestSessionStateAnimator : public SessionStateAnimator {
 public:
  TestSessionStateAnimator();

  TestSessionStateAnimator(const TestSessionStateAnimator&) = delete;
  TestSessionStateAnimator& operator=(const TestSessionStateAnimator&) = delete;

  ~TestSessionStateAnimator() override;

  // Resets the current animation epoch back to 0 and aborts all currently
  // active animations.
  void ResetAnimationEpoch();

  // Advances all contained animations by the specified |duration|.  Any
  // animations that will have completed after |duration| will have its
  // callback called.
  void Advance(const base::TimeDelta& duration);

  // Simulates running all of the contained animations to completion.  Each
  // contained AnimationSequence will have OnAnimationCompleted called if
  // |completed_successfully| is true and OnAnimationAborted called if false.
  void CompleteAnimations(int animation_epoch, bool completed_successfully);

  // Convenience method that calls CompleteAnimations with the last
  // |animation_epoch|.  In effect this will complete all animations.
  // See CompleteAnimations for more documenation on |completed_succesffully|.
  void CompleteAllAnimations(bool completed_successfully);

  // Returns true if there is an animation active with |type| for the given
  // |container|.
  bool IsContainerAnimated(SessionStateAnimator::Container container,
                           SessionStateAnimator::AnimationType type) const;

  // Returns true if there is an animation active with |type| for all the given
  // containers specified by |container_mask|.
  bool AreContainersAnimated(int container_mask,
                             SessionStateAnimator::AnimationType type) const;

  // Returns the number of active animations.
  size_t GetAnimationCount() const;

  // ash::SessionStateAnimator:
  void StartAnimation(int container_mask,
                      AnimationType type,
                      AnimationSpeed speed) override;
  void StartAnimationWithCallback(int container_mask,
                                  AnimationType type,
                                  AnimationSpeed speed,
                                  base::OnceClosure callback) override;
  AnimationSequence* BeginAnimationSequence(
      AnimationCallback callback) override;
  bool IsWallpaperHidden() const override;
  void ShowWallpaper() override;
  void HideWallpaper() override;

  void AbortAnimations(int container_mask);

 private:
  class AnimationSequence;
  friend class AnimationSequence;

  // Data structure to track the currently active animations and their
  // callbacks.
  struct ActiveAnimation {
    ActiveAnimation(int animation_epoch,
                    base::TimeDelta duration,
                    SessionStateAnimator::Container container,
                    AnimationType type,
                    AnimationSpeed speed,
                    base::OnceClosure success_callback,
                    base::OnceClosure failed_callback);
    ActiveAnimation(ActiveAnimation&& other);
    ActiveAnimation& operator=(ActiveAnimation&& other);
    virtual ~ActiveAnimation();

    // The time epoch that this animation was scheduled.
    int animation_epoch;

    // The time remaining for this animation.
    base::TimeDelta remaining_duration;

    // The container which is being animated.
    SessionStateAnimator::Container container;

    // The animation type that is being done.
    AnimationType type;

    // The speed at which the animation is being done.
    AnimationSpeed speed;

    // The callback to be invoked upon a successful completion.
    base::OnceClosure success_callback;

    // The callback to be invoked upon an unsuccessful completion.
    base::OnceClosure failed_callback;
  };

  typedef std::vector<ActiveAnimation> AnimationList;
  typedef std::map<SessionStateAnimator::Container, AnimationList>
      ActiveAnimationsMap;

  // Starts an animation in the |animation_sequence| for each container
  // specified by |container_mask| with the given |type| and |speed|.
  virtual void StartAnimationInSequence(int container_mask,
                                        AnimationType type,
                                        AnimationSpeed speed,
                                        AnimationSequence* animation_sequence);

  // Adds a single animation to the currently active animations.  If an
  // animation is already active for the given |container| then it will be
  // replaced by the new one.  The existing animation will be aborted by calling
  // OnAnimationAborted.
  void AddAnimation(SessionStateAnimator::Container container,
                    AnimationType type,
                    AnimationSpeed speed,
                    base::OnceClosure success_callback,
                    base::OnceClosure failed_callback);

  // If an animation is currently active for the given |container| it will be
  // aborted by invoking OnAnimationAborted and removed from the list of active
  // animations.
  void AbortAnimation(SessionStateAnimator::Container container);

  // Used for easy iteration over all the containers.
  static const SessionStateAnimator::Container kAllContainers[];

  // A map of currently active animations.
  ActiveAnimationsMap active_animations_;

  // A time counter that tracks the last scheduled animation or animation
  // sequence.
  int last_animation_epoch_ = 0;

  // Tracks whether the wallpaper is hidden or not.
  bool is_wallpaper_hidden_ = false;
};

}  // namespace ash

#endif  // ASH_WM_TEST_TEST_SESSION_STATE_ANIMATOR_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_ANIMATION_REGISTRY_H_
#define ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_ANIMATION_REGISTRY_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/system/progress_indicator/progress_icon_animation.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"

namespace ash {

// A registry for progress indicator animations.
//
// Since animations are owned by the registry, they can be shared across
// different UI components as well have a lifetime which is decoupled from UI
// component lifetime.
//
// Supported animation types:
//   * Progress icon animation - independently drive the animation of properties
//     for a progress indicator's inner icon, as opposed to progress ring
//     animations which independently drive the animation of properties for a
//     progress indicator's outer ring.
//   * Progress ring animation - independently drive the animation of properties
//     for a progress indicator's outer ring, as opposed to progress icon
//     animations which independently drive the animation of properties for a
//     progress indicator's inner icon.
class ASH_EXPORT ProgressIndicatorAnimationRegistry {
 public:
  ProgressIndicatorAnimationRegistry();
  ProgressIndicatorAnimationRegistry(
      const ProgressIndicatorAnimationRegistry&) = delete;
  ProgressIndicatorAnimationRegistry& operator=(
      const ProgressIndicatorAnimationRegistry&) = delete;
  ~ProgressIndicatorAnimationRegistry();

  using AnimationKey = intptr_t;

  // Returns the specified `ptr` as an animation key.
  static AnimationKey AsAnimationKey(const void* ptr);

  using ProgressIconAnimationChangedCallbackList =
      base::RepeatingCallbackList<void(ProgressIconAnimation*)>;

  // Adds the specified `callback` to be notified of changes to the progress
  // icon animation associated with the specified `key`. The `callback` will
  // continue to receive events so long as both `this` and the returned
  // subscription exist.
  base::CallbackListSubscription AddProgressIconAnimationChangedCallbackForKey(
      AnimationKey key,
      ProgressIconAnimationChangedCallbackList::CallbackType callback);

  using ProgressRingAnimationChangedCallbackList =
      base::RepeatingCallbackList<void(ProgressRingAnimation*)>;

  // Adds the specified `callback` to be notified of changes to the progress
  // ring animation associated with the specified `key`. The `callback` will
  // continue to receive events so long as both `this` and the returned
  // subscription exist.
  base::CallbackListSubscription AddProgressRingAnimationChangedCallbackForKey(
      AnimationKey key,
      ProgressRingAnimationChangedCallbackList::CallbackType callback);

  // Returns the progress icon animation registered for the specified `key`.
  // NOTE: This may return `nullptr` if no such animation is registered.
  ProgressIconAnimation* GetProgressIconAnimationForKey(AnimationKey key);

  // Returns the progress ring animation registered for the specified `key`.
  // NOTE: This may return `nullptr` if no such animation is registered.
  ProgressRingAnimation* GetProgressRingAnimationForKey(AnimationKey key);

  // Sets and returns the progress icon animation registered for the specified
  // `key`. NOTE: `animation` may be `nullptr` to unregister `key`.
  ProgressIconAnimation* SetProgressIconAnimationForKey(
      AnimationKey key,
      std::unique_ptr<ProgressIconAnimation> animation);

  // Sets and returns the progress ring animation registered for the specified
  // `key`. NOTE: `animation` may be `nullptr` to unregister `key`.
  ProgressRingAnimation* SetProgressRingAnimationForKey(
      AnimationKey key,
      std::unique_ptr<ProgressRingAnimation> animation);

  // Erases all animations for all keys.
  void EraseAllAnimations();

  // Erases all animations for the specified `key`.
  void EraseAllAnimationsForKey(AnimationKey key);

  // Erases all animations for all keys for which the specified `predicate`
  // returns `true`.
  void EraseAllAnimationsForKeyIf(
      base::FunctionRef<bool(AnimationKey key)> predicate);

 private:
  // Mapping of keys to their associated progress icon animations.
  std::map<AnimationKey, std::unique_ptr<ProgressIconAnimation>>
      icon_animations_by_key_;

  // Mapping of keys to their associated icon animation changed callback lists.
  // Whenever an animation for a given key is changed, the callback list for
  // that key will be notified.
  std::map<AnimationKey, ProgressIconAnimationChangedCallbackList>
      icon_animation_changed_callback_lists_by_key_;

  // Mapping of keys to their associated progress ring animations.
  std::map<AnimationKey, std::unique_ptr<ProgressRingAnimation>>
      ring_animations_by_key_;

  // Mapping of keys to their associated ring animation changed callback lists.
  // Whenever an animation for a given key is changed, the callback list for
  // that key will be notified.
  std::map<AnimationKey, ProgressRingAnimationChangedCallbackList>
      ring_animation_changed_callback_lists_by_key_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_ANIMATION_REGISTRY_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_ANIMATION_REGISTRY_H_
#define ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_ANIMATION_REGISTRY_H_

#include "base/callback_forward.h"

namespace ash {

class HoldingSpaceProgressIconAnimation;
class HoldingSpaceProgressRingAnimation;

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
class ProgressIndicatorAnimationRegistry {
 public:
  using ProgressIconAnimationChangedCallbackList =
      base::RepeatingCallbackList<void(HoldingSpaceProgressIconAnimation*)>;

  // Adds the specified `callback` to be notified of changes to the progress
  // icon animation associated with the specified `key`. The `callback` will
  // continue to receive events so long as both `this` and the returned
  // subscription exist.
  virtual base::CallbackListSubscription
  AddProgressIconAnimationChangedCallbackForKey(
      const void* key,
      ProgressIconAnimationChangedCallbackList::CallbackType callback) = 0;

  using ProgressRingAnimationChangedCallbackList =
      base::RepeatingCallbackList<void(HoldingSpaceProgressRingAnimation*)>;

  // Adds the specified `callback` to be notified of changes to the progress
  // ring animation associated with the specified `key`. The `callback` will
  // continue to receive events so long as both `this` and the returned
  // subscription exist.
  virtual base::CallbackListSubscription
  AddProgressRingAnimationChangedCallbackForKey(
      const void* key,
      ProgressRingAnimationChangedCallbackList::CallbackType callback) = 0;

  // Returns the progress icon animation registered for the specified `key`.
  // NOTE: This may return `nullptr` if no such animation is registered.
  virtual HoldingSpaceProgressIconAnimation* GetProgressIconAnimationForKey(
      const void* key) = 0;

  // Returns the progress ring animation registered for the specified `key`.
  // NOTE: This may return `nullptr` if no such animation is registered.
  virtual HoldingSpaceProgressRingAnimation* GetProgressRingAnimationForKey(
      const void* key) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PROGRESS_INDICATOR_PROGRESS_INDICATOR_ANIMATION_REGISTRY_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ANIMATION_REGISTRY_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ANIMATION_REGISTRY_H_

#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/scoped_observation.h"

namespace ash {

class HoldingSpaceProgressRingAnimation;

// A lazily initialized singleton registry for holding space animations. Since
// registered animations are owned by the singleton, they can be shared across
// different UI components as well have a lifetime which is decoupled from UI
// component lifetime. Note that the singleton may only exist while `Shell` is
// alive and will automatically delete itself when `Shell` is being destroyed.
class HoldingSpaceAnimationRegistry : public ShellObserver {
 public:
  HoldingSpaceAnimationRegistry(const HoldingSpaceAnimationRegistry&) = delete;
  HoldingSpaceAnimationRegistry& operator=(
      const HoldingSpaceAnimationRegistry&) = delete;
  ~HoldingSpaceAnimationRegistry() override;

  // Returns the lazily initialized singleton registry instance. The singleton
  // may only exist while `Shell` is alive and will automatically delete itself
  // when `Shell` is being destroyed.
  static HoldingSpaceAnimationRegistry* GetInstance();

  using ProgressRingAnimationChangedCallbackList =
      base::RepeatingCallbackList<void(HoldingSpaceProgressRingAnimation*)>;

  // Adds the specified `callback` to be notified of changes to the progress
  // ring animation associated with the specified `key`. The `callback` will
  // continue to receive events so long as both `this` and the returned
  // subscription exist.
  ProgressRingAnimationChangedCallbackList::Subscription
  AddProgressRingAnimationChangedCallbackForKey(
      const void* key,
      ProgressRingAnimationChangedCallbackList::CallbackType callback);

  // Returns the progress ring animation registered for the specified `key`.
  // For cumulative progress, the animation is keyed on a pointer to the holding
  // space controller. For individual item progress, the animation is keyed on a
  // pointer to the holding space item itself.
  // NOTE: This may return `nullptr` if no such animation is registered.
  HoldingSpaceProgressRingAnimation* GetProgressRingAnimationForKey(
      const void* key);

 private:
  HoldingSpaceAnimationRegistry();

  // ShellObserver:
  void OnShellDestroying() override;

  // The delegate responsible for creating and curating progress ring animations
  // based on holding space model state.
  class ProgressRingAnimationDelegate;
  std::unique_ptr<ProgressRingAnimationDelegate>
      progress_ring_animation_delegate_;

  base::ScopedObservation<Shell,
                          ShellObserver,
                          &Shell::AddShellObserver,
                          &Shell::RemoveShellObserver>
      shell_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ANIMATION_REGISTRY_H_

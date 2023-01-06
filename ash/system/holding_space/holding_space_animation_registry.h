// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ANIMATION_REGISTRY_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ANIMATION_REGISTRY_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"

namespace ash {

class Shell;

// A lazily initialized singleton registry for holding space animations.
//
// Since registered animations are owned by the singleton, they can be shared
// across different UI components as well have a lifetime which is decoupled
// from UI component lifetime. Note that the singleton may only exist while
// `Shell` is alive and will automatically delete itself when `Shell` is being
// destroyed.
//
// Supported animation types:
//   * Progress icon animation - see `ProgressIndicatorAnimationRegistry`.
//   * Progress ring animation - see `ProgressIndicatorAnimationRegistry`.
class ASH_EXPORT HoldingSpaceAnimationRegistry
    : public ProgressIndicatorAnimationRegistry,
      public ShellObserver {
 public:
  HoldingSpaceAnimationRegistry(const HoldingSpaceAnimationRegistry&) = delete;
  HoldingSpaceAnimationRegistry& operator=(
      const HoldingSpaceAnimationRegistry&) = delete;
  ~HoldingSpaceAnimationRegistry() override;

  // Returns the lazily initialized singleton registry instance. The singleton
  // may only exist while `Shell` is alive and will automatically delete itself
  // when `Shell` is being destroyed.
  static HoldingSpaceAnimationRegistry* GetInstance();

 private:
  HoldingSpaceAnimationRegistry();

  // ShellObserver:
  void OnShellDestroying() override;

  // The delegate responsible for creating and curating progress indicator
  // animations based on holding space model state.
  class ProgressIndicatorAnimationDelegate;
  std::unique_ptr<ProgressIndicatorAnimationDelegate>
      progress_indicator_animation_delegate_;

  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ANIMATION_REGISTRY_H_

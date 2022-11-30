// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCOPED_ANIMATION_DISABLER_H_
#define ASH_SCOPED_ANIMATION_DISABLER_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

// Helper class to perform window state changes without animations. Used to hide
// /show/minimize windows without having their animation interfere with the ones
// this class is in charge of.
class ASH_EXPORT ScopedAnimationDisabler {
 public:
  explicit ScopedAnimationDisabler(aura::Window* window);
  ScopedAnimationDisabler(const ScopedAnimationDisabler&) = delete;
  ScopedAnimationDisabler& operator=(const ScopedAnimationDisabler&) = delete;
  ~ScopedAnimationDisabler();

 private:
  aura::Window* const window_;
  bool needs_disable_ = false;
};

}  // namespace ash

#endif  // ASH_SCOPED_ANIMATION_DISABLER_H_

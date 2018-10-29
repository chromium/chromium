// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCOPED_ANIMATION_DISABLER_H_
#define ASH_SCOPED_ANIMATION_DISABLER_H_

#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {

// Helper class to perform window state changes without animations. Used to hide
// /show/minimize windows without having their animation interfere with the ones
// this class is in charge of.
class ScopedAnimationDisabler {
 public:
  explicit ScopedAnimationDisabler(aura::Window* window);
  ~ScopedAnimationDisabler();

 private:
  aura::Window* window_;
  bool needs_disable_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedAnimationDisabler);
};

}  // namespace ash

#endif  // ASH_SCOPED_ANIMATION_DISABLER_H_

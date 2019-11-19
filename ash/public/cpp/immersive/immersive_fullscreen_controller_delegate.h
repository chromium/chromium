// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_FULLSCREEN_CONTROLLER_DELEGATE_H_
#define ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_FULLSCREEN_CONTROLLER_DELEGATE_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace gfx {
class Rect;
}

namespace ash {

class ASH_PUBLIC_EXPORT ImmersiveFullscreenControllerDelegate {
 public:
  // Called when a reveal of the top-of-window views starts.
  virtual void OnImmersiveRevealStarted() = 0;

  // Called when the top-of-window views have finished closing. This call
  // implies a visible fraction of 0. SetVisibleFraction(0) may not be called
  // prior to OnImmersiveRevealEnded().
  virtual void OnImmersiveRevealEnded() = 0;

  // Called as a result of enabling immersive fullscreen via SetEnabled().
  virtual void OnImmersiveFullscreenEntered() = 0;

  // Called as a result of disabling immersive fullscreen via SetEnabled().
  virtual void OnImmersiveFullscreenExited() = 0;

  // Called to update the fraction of the top-of-window views height which is
  // visible.
  virtual void SetVisibleFraction(double visible_fraction) = 0;

  // Returns a list of rects whose union makes up the top-of-window views.
  // The returned list is used for hittesting when the top-of-window views
  // are revealed. GetVisibleBoundsInScreen() must return a valid value when
  // not in immersive fullscreen for the sake of SetupForTest().
  virtual std::vector<gfx::Rect> GetVisibleBoundsInScreen() const = 0;

  // Re-layout the frame. Called when |EnableForWidget| is called
  // but full state did not change.
  virtual void Relayout() {}

 protected:
  virtual ~ImmersiveFullscreenControllerDelegate() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_FULLSCREEN_CONTROLLER_DELEGATE_H_

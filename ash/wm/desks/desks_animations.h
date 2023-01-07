// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_ANIMATIONS_H_
#define ASH_WM_DESKS_DESKS_ANIMATIONS_H_

namespace aura {
class Window;
}  // namespace aura

namespace ash {

namespace desks_animations {

// Animates the given |root| using a bounce-like animation to indicate that
// there are no more desks in the direction the user is trying to go to.
// |going_left| is true when the user requests to switch to a desk on the left
// of the currently active one.
void PerformHitTheWallAnimation(aura::Window* root, bool going_left);

// Recreates the layer tree of the given |window| and animates its old layer
// tree offscreen in the direction of the target desk indicated by |going_left|.
// After this function, |window| can be moved safely immediately to the target
// desk without having to wait for the animation to finish, since we're
// animating a completely separate layer tree. |window| cannot be visible on all
// desks.
// Note: This animation should not be performed on windows in overview.
void PerformWindowMoveToDeskAnimation(aura::Window* window, bool going_left);

}  // namespace desks_animations

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_ANIMATIONS_H_

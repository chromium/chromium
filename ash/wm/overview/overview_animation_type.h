// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_

namespace ash {

// Enumeration of the different overview mode animations.
enum OverviewAnimationType {
  OVERVIEW_ANIMATION_NONE,
  // Used to fade in the close button and label.
  OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
  // Used to fade out the label.
  OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT,
  // Used to position windows when entering/exiting overview mode and when a
  // window is closed while overview mode is active.
  OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER,
  OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW,
  OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT,
  // Used to add an item to an active overview session using the spawn
  // animation.
  OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW,
  // Used to restore windows to their original position when exiting overview
  // mode.
  OVERVIEW_ANIMATION_RESTORE_WINDOW,
  // Same as RESTORE_WINDOW but apply the target at the end of the animation.
  OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO,
  // Used to animate scaling down of a window that is about to get closed while
  // overview mode is active.
  OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM,
  // Used to animate hiding of a window that is closed while overview mode is
  // active.
  OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM,
  // Used to animate windows upon entering or exiting overview mode to or from
  // the home launcher.
  OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER,
  OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER,
  // Used to fade the drop target when dragging an application.
  OVERVIEW_ANIMATION_DROP_TARGET_FADE,
  // Used to fade in the label which tells users they are in overview mode with
  // no window in and out.
  OVERVIEW_ANIMATION_NO_RECENTS_FADE,
  // Used to animate the overview highlight which is activated by using tab or
  // the arrow keys.
  OVERVIEW_ANIMATION_SELECTION_WINDOW,
  // Used to animate the clipping of the windows frame header.
  OVERVIEW_ANIMATION_FRAME_HEADER_CLIP,
  // Used to fade in all windows when window drag starts or during window drag.
  OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG,
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_TYPES_H_
#define ASH_WM_OVERVIEW_OVERVIEW_TYPES_H_

namespace ash {

// Enumeration of the different overview mode animations.
enum OverviewAnimationType {
  OVERVIEW_ANIMATION_NONE,
  // Used to fade in the close button and label.
  OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
  // Used to fade out the label.
  OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT,
  // Used to show the informed restore dialog when entering Overview.
  OVERVIEW_ANIMATION_SHOW_INFORMED_RESTORE_DIALOG_ON_ENTER,
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
  // Used to fade in all windows when window drag starts or during window drag.
  OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG,
  // Used to fade out the saved desk grid when exiting overview mode.
  OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_SAVED_DESK_GRID_FADE_OUT,
  // Used to fade out the birch bar when existing overview mode.
  OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_BIRCH_BAR_FADE_OUT,
};

enum class OverviewTransition {
  kEnter,       // Entering overview.
  kInOverview,  // Already in overview.
  kExit         // Exiting overview.
};

// Enum describing the different ways overview can be entered or exited.
enum class OverviewEnterExitType {
  // The default way, window(s) animate from their initial bounds to the grid
  // bounds. Window(s) that are not visible to the user do not get animated.
  // This should always be the type when in clamshell mode.
  kNormal,
  // Used only when it's desired to enter overview mode immediately without
  // animations. It's used when entering overview by dragging a window from
  // the top of the screen or from the shelf, or by long pressing the overview
  // button tray. It's also used to address https://crbug.com/1027179. This
  // should not be used for exiting overview mode.
  kImmediateEnter,
  // Used when it's desired to enter overview mode immediately without
  // animations. Additionally, the overview controller will not automatically
  // move focus over to the overview focus widget (which is something that
  // happens on a timer with `kImmediateEnter`). Behaves otherwise like
  // `kImmediateEnter`.
  kImmediateEnterWithoutFocus,
  // Used only when it's desired to exit overview mode immediately without
  // animations. This is used when performing the desk switch animation when
  // the source desk is in overview mode, while the target desk is not.
  // This should not be used for entering overview mode.
  kImmediateExit,
  // Fades all windows in to enter overview. This can happen when
  // transitioning to overview from home screen (in a state where all windows
  // are minimized).
  kFadeInEnter,
  // Fades all windows out to exit overview (when going to a state where all
  // windows are minimized). This will minimize windows on exit if needed, so
  // that we do not need to add a delayed observer to handle minimizing the
  // windows after overview exit animations are finished.
  kFadeOutExit,
  // Allows for a smooth transition to and from overview mode. When this type
  // is used, overview mode will be entered immediately. However, the windows
  // will stay in their current position/state. As the user scrolls up and down
  // on the trackpad, each window will be put in an "in-between" state, between
  // their current and final state, according to the scroll offset.
  kContinuousAnimationEnterOnScrollUpdate,
  // Like `kNormal` but this is triggered from the full restore service when the
  // login work is still being completed. Birch uses this to determine what
  // timeout to use.
  kInformedRestore,
};

// Overview items have certain properties if their aspect ratio exceeds a
// threshold. This enum keeps track of which category the window falls into,
// based on its aspect ratio.
enum class OverviewItemFillMode {
  // Aspect ratio is between 1:2 and 2:1.
  kNormal,
  // Width to height ratio exceeds 2:1. The overview item will have a 2:1
  // aspect ratio. The window will maintain its aspect ratio and the rest of
  // the item will be filled with a backdrop.
  kLetterBoxed,
  // Width to height ratio exceeds 1:2. The overview item will have a 1:2
  // aspect ratio. The window will maintain its aspect ratio and the rest of
  // the item will be filled with a backdrop.
  kPillarBoxed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DeskBarVisibility)
enum class DeskBarVisibility {
  // Desk bar is shown in the first overview frame.
  kShownImmediately = 0,
  // Desk bar is shown after the first overview frame (usually after the
  // enter-overview animation is complete).
  kShownAfterFirstFrame = 1,
  // Desk bar was never shown during the overview session.
  kNotShown = 2,
  kMaxValue = kNotShown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:DeskBarVisibility)

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TYPES_H_

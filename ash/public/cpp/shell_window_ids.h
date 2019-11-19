// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELL_WINDOW_IDS_H_
#define ASH_PUBLIC_CPP_SHELL_WINDOW_IDS_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"

// Declarations of ids of special shell windows.

namespace ash {

enum ShellWindowId {
  // Used to indicate no shell window id.
  kShellWindowId_Invalid = -1,

  // The screen rotation container in between root window and its children, used
  // for screen rotation animation.
  kShellWindowId_ScreenRotationContainer = 0,

  // The magnified container which contains everything that would be magnified
  // when docked magnifier is enabled.
  kShellWindowId_MagnifiedContainer,

  // The container for the Docked Magnifier viewport widget and the separator.
  kShellWindowId_DockedMagnifierContainer,

  // A higher-level container that holds all of the containers stacked below
  // kShellWindowId_LockScreenContainer.  Only used by PowerButtonController for
  // animating lower-level containers.
  kShellWindowId_NonLockScreenContainersContainer,

  // A higher-level container that holds containers that hold lock-screen
  // windows.  Only used by PowerButtonController for animating lower-level
  // containers.
  kShellWindowId_LockScreenContainersContainer,

  // A higher-level container that holds containers that hold
  // lock-screen-related windows (which are displayed regardless of the screen
  // lock state, effectively containers stacked above
  // kShellWindowId_LockSystemModalContainer). Used by the shelf, status area,
  // virtual keyboard, settings bubble, menus, etc.  Also used by the
  // PowerButtonController for animating lower-level containers.
  kShellWindowId_LockScreenRelatedContainersContainer,

  // A container used for windows of WINDOW_TYPE_CONTROL that have no parent.
  // This container is not visible.
  kShellWindowId_UnparentedControlContainer,

  // The wallpaper (desktop background) window.
  kShellWindowId_WallpaperContainer,

  // The containers for standard top-level windows per active desks.
  // * Notes:
  //   - There are no direct mapping between `kShellWindowId_DeskContainerA` and
  //     Desk 1, or `kShellWindowId_DeskContainerB` and Desk 2. The containers
  //     are reused as desks are created and deleted.
  //   - **DO NOT** use these container IDs directly, instead use
  //     `desks_util::GetActiveDeskContainerId()`.
  // TODO(afakhry): Rename this container, unexpose it, and add the rest of the
  // containers.
  kShellWindowId_DefaultContainerDeprecated,
  kShellWindowId_DeskContainerB,
  kShellWindowId_DeskContainerC,
  kShellWindowId_DeskContainerD,

  // The container for top-level windows with the 'always-on-top' flag set.
  kShellWindowId_AlwaysOnTopContainer,

  // The container for the app list.
  kShellWindowId_AppListContainer,

  // The container for the home screen, e.g. the app list in tablet mode.
  kShellWindowId_HomeScreenContainer,

  // The container for the PIP window.
  kShellWindowId_PipContainer,

  // The parent container that holds the ARC IME container and windows created
  // by ARC IME other than the virtual keyboard window.
  // This container window is to ensure that the ARC IME window is stacked above
  // top-level windows and the app list window but below the shelf.
  kShellWindowId_ArcImeWindowParentContainer,

  // The container for Virtual Keyboard from ARC IMEs.
  kShellWindowId_ArcVirtualKeyboardContainer,

  // The container for the shelf.
  kShellWindowId_ShelfContainer,

  // The container for bubbles which float over the shelf.
  kShellWindowId_ShelfBubbleContainer,

  // The container for user-specific modal windows.
  kShellWindowId_SystemModalContainer,

  // The container for the lock screen wallpaper (lock screen background).
  kShellWindowId_LockScreenWallpaperContainer,

  // The container for the lock screen.
  kShellWindowId_LockScreenContainer,

  // The container for windows that handle lock tray actions (e.g. new note
  // action). The action handler container's children should be visible on lock
  // screen, but only when an action is being handled - i.e. action handling
  // state is either:
  //  *  active - the container is stacked above LockScreenContainer
  //  *  background - the container is stacked below LockScreenContainer
  kShellWindowId_LockActionHandlerContainer,

  // The container for the lock screen modal windows.
  kShellWindowId_LockSystemModalContainer,

  // The container for shelf control widgets (navigation, hotseat).
  kShellWindowId_ShelfControlContainer,

  // The container for the status area.
  kShellWindowId_StatusContainer,

  // A parent container that holds the virtual keyboard container and ime
  // windows if any. This is to ensure that the virtual keyboard or ime window
  // is stacked above most containers but below the mouse cursor and the power
  // off animation.
  kShellWindowId_ImeWindowParentContainer,

  // The virtual keyboard container.
  kShellWindowId_VirtualKeyboardContainer,

  // The container for menus.
  kShellWindowId_MenuContainer,

  // The container for drag/drop images and tooltips.
  kShellWindowId_DragImageAndTooltipContainer,

  // The container for the fullscreen power button menu.
  kShellWindowId_PowerMenuContainer,

  // The container for bubbles briefly overlaid onscreen to show settings
  // changes (volume, brightness, input method bubbles, etc.).
  kShellWindowId_SettingBubbleContainer,

  // Contains special accessibility windows that can inset the display work area
  // (e.g. the ChromeVox spoken feedback window).
  // TODO(jamescook): Consolidate this with DockedMagnifierContainer.
  kShellWindowId_AccessibilityPanelContainer,

  // The container for the Autoclick bubble that overlays the work area and any
  // menus and bubbles, but appears under the Autoclick mouse UX in
  // kShellWindowId_OverlayContainer. Autoclick needs to work with dialogs and
  // menus, so it must be shown above kShellWindowId_SettingBubbleContainer to
  // allow the user to access these settings. However, the Autoclick bubble has
  // buttons with tooltips which must be shown above the Autoclick bubble, so it
  // must be under kShellWindowId_DragImageAndTooltipContainer.
  kShellWindowId_AutoclickContainer,

  // The container for special components overlaid onscreen, such as the
  // region selector for partial screenshots.
  kShellWindowId_OverlayContainer,

  // The container for mouse cursor.
  kShellWindowId_MouseCursorContainer,

  // The container for an image that should stay on top of everything except for
  // the power off animation.
  kShellWindowId_AlwaysOnTopWallpaperContainer,

  // The topmost container, used for power off animation.
  kShellWindowId_PowerButtonAnimationContainer,

  kShellWindowId_MinContainer = kShellWindowId_ScreenRotationContainer,
  kShellWindowId_MaxContainer = kShellWindowId_PowerButtonAnimationContainer,
};

// Special shell windows that are not containers.
enum NonContainerWindowId {
  // The window created by PhantomWindowController or DragWindowController.
  kShellWindowId_PhantomWindow = kShellWindowId_MaxContainer + 1
};

// A list of system modal container IDs. The order of the list is important that
// the more restrictive container appears before the less restrictive ones.
constexpr int kSystemModalContainerIds[] = {
    kShellWindowId_LockSystemModalContainer,
    kShellWindowId_SystemModalContainer};

// Returns the list of container ids of containers which may contain windows
// that need to be activated. this list is ordered by the activation order; that
// is, windows in containers appearing earlier in the list are activated before
// windows in containers appearing later in the list. This list is used by
// AshFocusRules to determine which container to start the search from when
// looking for the next activatable window.
ASH_PUBLIC_EXPORT std::vector<int> GetActivatableShellWindowIds();

// Returns true if |id| is in |kActivatableShellWindowIds|.
ASH_PUBLIC_EXPORT bool IsActivatableShellWindowId(int id);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELL_WINDOW_IDS_H_

// Copyright 2012 The Chromium Authors
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

  // This container is used for animations which take a screenshot of the
  // contents, place them on top of the root and animate the screenshot layer.
  // We can't take a screenshot of the root itself, otherwise subsequent
  // screenshots will screenshot previous screenshots.
  kShellWindowId_ScreenAnimationContainer = 0,

  // The container that displays booting animations.
  kShellWindowId_BootingAnimationContainer,

  // The magnified container which contains everything that would be magnified
  // when docked magnifier is enabled.
  kShellWindowId_MagnifiedContainer,

  // The container for the Docked Magnifier viewport widget and the separator.
  kShellWindowId_DockedMagnifierContainer,

  // A higher-level container that holds all of the containers stacked below
  // kShellWindowId_LockScreenContainer.  Only used by PowerButtonController for
  // animating lower-level containers and AccessibilityController for hiding
  // non-lock screen windows from Accessibility when the user session is
  // blocked.
  kShellWindowId_NonLockScreenContainersContainer,

  // A higher-level container that holds containers that hold lock-screen
  // windows. Only used by PowerButtonController for animating lower-level
  // containers.
  kShellWindowId_LockScreenContainersContainer,

  // A higher-level container that holds containers that hold
  // lock-screen-related windows (which are displayed regardless of the screen
  // lock state, effectively containers stacked above
  // kShellWindowId_LockSystemModalContainer). Used by the shelf, status area,
  // virtual keyboard, settings bubble, menus, etc. Also used by the
  // PowerButtonController for animating lower-level containers.
  kShellWindowId_LockScreenRelatedContainersContainer,

  // A container used for windows that temporarily have no parent. It is
  // expected the windows will get parented to another container shortly after.
  // This container is not visible.
  kShellWindowId_UnparentedContainer,

  // The wallpaper (desktop background) window.
  kShellWindowId_WallpaperContainer,

  // A container for the windows that will be included in the shutdown
  // screenshot. Including desk containers, float and always on top containers.
  // Note: Only created if the forest feature is enabled.
  kShellWindowId_ShutdownScreenshotContainer,

  // The containers for standard top-level windows per active desks.
  // * Notes:
  //   - There are no direct mapping between `kShellWindowId_DeskContainerA` and
  //     Desk 1, or `kShellWindowId_DeskContainerB` and Desk 2. The containers
  //     are reused as desks are created and deleted.
  //   - Keep the desk container IDs sequential here.
  //   - **DO NOT** use these container IDs directly, instead use
  //     `desks_util::GetActiveDeskContainerId()`.
  kShellWindowId_DeskContainerA,
  kShellWindowId_DeskContainerB,
  kShellWindowId_DeskContainerC,
  kShellWindowId_DeskContainerD,
  kShellWindowId_DeskContainerE,
  kShellWindowId_DeskContainerF,
  kShellWindowId_DeskContainerG,
  kShellWindowId_DeskContainerH,
  kShellWindowId_DeskContainerI,
  kShellWindowId_DeskContainerJ,
  kShellWindowId_DeskContainerK,
  kShellWindowId_DeskContainerL,
  kShellWindowId_DeskContainerM,
  kShellWindowId_DeskContainerN,
  kShellWindowId_DeskContainerO,
  kShellWindowId_DeskContainerP,

  // The container for top-level windows with the 'always-on-top' flag set.
  kShellWindowId_AlwaysOnTopContainer,

  // The container for the floating window.
  kShellWindowId_FloatContainer,

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

  // The container for UI on the shelf (shelf, navigation, hotseat,
  // status area).
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

  // A parent container that holds the virtual keyboard container and ime
  // windows if any. This is to ensure that the virtual keyboard or ime window
  // is stacked above most containers but below the mouse cursor and the power
  // off animation.
  kShellWindowId_ImeWindowParentContainer,

  // The virtual keyboard container.
  kShellWindowId_VirtualKeyboardContainer,

  // The container for menus.
  kShellWindowId_MenuContainer,

  // The container for drag/drop images, tooltips and widgets that are tagged
  // with ui::ZOrderLevel::kSecuritySurface.
  kShellWindowId_DragImageAndTooltipContainer,

  // The container for the fullscreen power button menu.
  kShellWindowId_PowerMenuContainer,

  // The container for bubbles briefly overlaid onscreen to show settings
  // changes (volume, brightness, input method bubbles, etc.), tray bubbles and
  // notifier elements such as notification popups, toasts and system nudges.
  kShellWindowId_SettingBubbleContainer,

  // The container for the live caption window.
  kShellWindowId_LiveCaptionContainer,

  // The container for help bubbles which are anchored to views for the purpose
  // of user education. In the case of the Welcome Tour, which walks new users
  // through ChromeOS System UI, a background blur will be applied to the
  // container with a masked cut out for the help bubble anchor view.
  kShellWindowId_HelpBubbleContainer,

  // Contains special accessibility windows that can inset the display work area
  // (e.g. the ChromeVox spoken feedback window).
  // TODO(jamescook): Consolidate this with DockedMagnifierContainer.
  kShellWindowId_AccessibilityPanelContainer,

  // The container for accessibility bubbles that overlay the work area and any
  // other menus and bubbles, but appear under the Autoclick mouse UX in
  // kShellWindowId_OverlayContainer. Both Autoclick and Switch Access have
  // bubbles that appear in this layer. These features need to work with dialogs
  // and menus, so they must be shown above
  // kShellWindowId_SettingBubbleContainer to allow the user to access these
  // settings. However, these bubbles may have buttons with tooltips which must
  // be shown above the bubbles, so it must be under
  // kShellWindowId_DragImageAndTooltipContainer.
  // TODO(crbug/1076973): Investigate merging this container with
  // AccessibilityPanelContainer.
  kShellWindowId_AccessibilityBubbleContainer,

  // The container for special components overlaid onscreen, such as the
  // region selector for partial screenshots.
  kShellWindowId_OverlayContainer,

  // The container for ambient mode screen saver.
  kShellWindowId_AmbientModeContainer,

  // The container for mouse cursor.
  kShellWindowId_MouseCursorContainer,

  // The container for an image that should stay on top of everything except for
  // the power off animation.
  kShellWindowId_AlwaysOnTopWallpaperContainer,

  // The topmost container, used for power off animation.
  kShellWindowId_PowerButtonAnimationContainer,

  kShellWindowId_MinContainer = kShellWindowId_ScreenAnimationContainer,
  kShellWindowId_MaxContainer = kShellWindowId_PowerButtonAnimationContainer,
};

// Special shell windows that are not containers.
enum NonContainerWindowId {
  // The window created by PhantomWindowController or DragWindowController.
  kShellWindowId_PhantomWindow = kShellWindowId_MaxContainer + 1,

  // The window that shows a blue highlight on the edges of a selected display.
  // Only one window exists whenever the display settings page is open with
  // multiple displays connected.
  kShellWindowId_DisplayIdentificationHighlightWindow,

  // The window specified as the owner of the folder selection menu for capture
  // mode, which will be a transient window parent of the about to be created
  // dialog window. This is needed in order to prevent
  // |SelectFileDialogExtension| from favoring to parent the dialog to a browser
  // window (if one exists).
  kShellWindowId_CaptureModeFolderSelectionDialogOwner,

  // The window that notifies the user that an admin user was present on the
  // host device when the remote desktop session was curtained.
  kShellWindowId_AdminWasPresentNotificationWindow,
};

// A list of system modal container IDs. The order of the list is important that
// the more restrictive container appears before the less restrictive ones.
constexpr int kSystemModalContainerIds[] = {
    kShellWindowId_LockSystemModalContainer, kShellWindowId_HelpBubbleContainer,
    kShellWindowId_SystemModalContainer};

// Returns the list of container ids of containers which may contain windows
// that need to be activated. this list is ordered by the activation order; that
// is, windows in containers appearing earlier in the list are activated before
// windows in containers appearing later in the list. This list is used by
// AshFocusRules to determine which container to start the search from when
// looking for the next activatable window.
ASH_PUBLIC_EXPORT std::vector<int> GetActivatableShellWindowIds();

// Returns true if |id| is in GetActivatableShellWindowIds.
ASH_PUBLIC_EXPORT bool IsActivatableShellWindowId(int id);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELL_WINDOW_IDS_H_

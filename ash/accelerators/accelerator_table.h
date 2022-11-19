// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/fixed_flat_map.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

// The complete list of Ash accelerators is in ash/public/cpp/accelerators.h.
// This file mainly keeps track of special categories of accelerator.
//
// There are five classes of accelerators in Ash:
//
// Ash (OS) reserved:
// * Neither packaged apps nor web pages can cancel.
// * For example, power button.
// * See kReservedActions below.
//
// Ash (OS) preferred:
// * Fullscreen window can consume, but normal window can't.
// * For example, Alt-Tab window cycling.
// * See kPreferredActions below.
//
// Chrome OS system keys:
// * For legacy reasons, v1 apps can process and cancel. Otherwise handled
//   directly by Ash.
// * Brightness, volume control, etc.
// * See IsSystemKey() in ash/accelerators/accelerator_filter.cc.
//
// Browser reserved:
// * Packaged apps can cancel but web pages cannot.
// * For example, browser back and forward from first-row function keys.
// * See IsReservedCommandOrKey() in
//   chrome/browser/ui/browser_command_controller.cc.
//
// Browser non-reserved:
// * Both packaged apps and web pages can cancel.
// * For example, selecting tabs by number with Ctrl-1 to Ctrl-9.
// * See kAcceleratorMap in chrome/browser/ui/views/accelerator_table.cc.
//
// In particular, there is not an accelerator processing pass for Ash after
// the browser gets the accelerator.  See crbug.com/285308 for details.
//
// There are also various restrictions on accelerators allowed at the login
// screen, when running in "forced app mode" (like a kiosk), etc. See the
// various kActionsAllowed* below.

// Gathers the needed data to handle deprecated accelerators.
struct DeprecatedAcceleratorData {
  // The action that has deprecated accelerators.
  AcceleratorAction action;

  // The name of the UMA histogram that will be used to measure the deprecated
  // v.s. new accelerator usage.
  const char* uma_histogram_name;

  // The ID of the localized notification message to show to users informing
  // them about the deprecation.
  int notification_message_id;

  // The ID of the localized old deprecated shortcut key.
  int old_shortcut_id;

  // The ID of the localized new shortcut key.
  int new_shortcut_id;

  // Specifies whether the deprecated accelerator is still enabled to do its
  // associated action.
  bool deprecated_enabled;
};

// This will be used for the UMA stats to measure the how many users are using
// the old v.s. new accelerators.
enum DeprecatedAcceleratorUsage {
  DEPRECATED_USED = 0,     // The deprecated accelerator is used.
  NEW_USED,                // The new accelerator is used.
  DEPRECATED_USAGE_COUNT,  // Maximum value of this enum for histogram use.
};

// The list of the deprecated accelerators.
ASH_EXPORT extern const AcceleratorData kDeprecatedAccelerators[];
ASH_EXPORT extern const size_t kDeprecatedAcceleratorsLength;

// The list of the actions with deprecated accelerators and the needed data to
// handle them.
ASH_EXPORT extern const DeprecatedAcceleratorData kDeprecatedAcceleratorsData[];
ASH_EXPORT extern const size_t kDeprecatedAcceleratorsDataLength;

// Debug accelerators. Debug accelerators are only enabled when the "Debugging
// keyboard shortcuts" flag (--ash-debug-shortcuts) is enabled. Debug actions
// are always run (similar to reserved actions). Debug accelerators can be
// enabled in about:flags.
ASH_EXPORT extern const AcceleratorData kDebugAcceleratorData[];
ASH_EXPORT extern const size_t kDebugAcceleratorDataLength;

// Developer accelerators that are enabled only with the command-line switch
// --ash-dev-shortcuts. They are always run similar to reserved actions.
ASH_EXPORT extern const AcceleratorData kDeveloperAcceleratorData[];
ASH_EXPORT extern const size_t kDeveloperAcceleratorDataLength;

// Actions that should be handled very early in Ash unless the current target
// window is full-screen.
ASH_EXPORT extern const AcceleratorAction kPreferredActions[];
ASH_EXPORT extern const size_t kPreferredActionsLength;

// Actions that are always handled in Ash.
ASH_EXPORT extern const AcceleratorAction kReservedActions[];
ASH_EXPORT extern const size_t kReservedActionsLength;

// Actions allowed while user is not signed in or screen is locked.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[];
ASH_EXPORT extern const size_t kActionsAllowedAtLoginOrLockScreenLength;

// Actions allowed while screen is locked (in addition to
// kActionsAllowedAtLoginOrLockScreen).
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLockScreen[];
ASH_EXPORT extern const size_t kActionsAllowedAtLockScreenLength;

// Actions allowed while power menu is opened.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtPowerMenu[];
ASH_EXPORT extern const size_t kActionsAllowedAtPowerMenuLength;

// Actions allowed while a modal window is up.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtModalWindow[];
ASH_EXPORT extern const size_t kActionsAllowedAtModalWindowLength;

// Actions which may be repeated by holding an accelerator key.
ASH_EXPORT extern const AcceleratorAction kRepeatableActions[];
ASH_EXPORT extern const size_t kRepeatableActionsLength;

// Actions allowed in app mode or pinned mode.
ASH_EXPORT extern const AcceleratorAction
    kActionsAllowedInAppModeOrPinnedMode[];
ASH_EXPORT extern const size_t kActionsAllowedInAppModeOrPinnedModeLength;

// Actions that can be performed in pinned mode.
// In pinned mode, the action listed in this or "in app mode or pinned mode"
// table can be performed.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedInPinnedMode[];
ASH_EXPORT extern const size_t kActionsAllowedInPinnedModeLength;

// Actions that can be performed in app mode.
// In app mode, the action listed in this or "in app mode or pinned mode" table
// can be performed.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedInAppMode[];
ASH_EXPORT extern const size_t kActionsAllowedInAppModeLength;

// Actions that require at least 1 window.
ASH_EXPORT extern const AcceleratorAction kActionsNeedingWindow[];
ASH_EXPORT extern const size_t kActionsNeedingWindowLength;

// Actions that can be performed while keeping the menu open.
ASH_EXPORT extern const AcceleratorAction kActionsKeepingMenuOpen[];
ASH_EXPORT extern const size_t kActionsKeepingMenuOpenLength;

// Actions that are duplicated with browser shortcuts.
ASH_EXPORT extern const AcceleratorAction kActionsDuplicatedWithBrowser[];
ASH_EXPORT extern const size_t kActionsDuplicatedWithBrowserLength;

// Actions that are interceptable by browser.
// These actions are ash's shortcuts, but they are sent to the browser
// once in order to make it interceptable by webpage/apps.
ASH_EXPORT extern const AcceleratorAction kActionsInterceptableByBrowser[];
ASH_EXPORT extern const size_t kActionsInterceptableByBrowserLength;

// A map between accelerator action id and accelerator description ID.
// Adding a new accelerator must add a new entry to this map.
ASH_EXPORT constexpr auto kAcceleratorActionToStringIdMap = base::MakeFixedFlatMap<
    AcceleratorAction,
    int>({
    {BRIGHTNESS_DOWN, IDS_ASH_ACCELERATOR_ACTION_BRIGHTNESS_DOWN},
    {BRIGHTNESS_UP, IDS_ASH_ACCELERATOR_ACTION_BRIGHTNESS_UP},
    {CYCLE_BACKWARD_MRU, IDS_ASH_ACCELERATOR_ACTION_CYCLE_BACKWARD_MRU},
    {CYCLE_FORWARD_MRU, IDS_ASH_ACCELERATOR_ACTION_CYCLE_FORWARD_MRU},
    {DESKS_ACTIVATE_DESK_LEFT,
     IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE_DESK_LEFT},
    {DESKS_ACTIVATE_DESK_RIGHT,
     IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE_DESK_RIGHT},
    {DESKS_MOVE_ACTIVE_ITEM_LEFT,
     IDS_ASH_ACCELERATOR_ACTION_DESKS_MOVE_ACTIVE_ITEM_LEFT},
    {DESKS_MOVE_ACTIVE_ITEM_RIGHT,
     IDS_ASH_ACCELERATOR_ACTION_DESKS_MOVE_ACTIVE_ITEM_RIGHT},
    {DESKS_NEW_DESK, IDS_ASH_ACCELERATOR_ACTION_DESKS_NEW_DESK},
    {DESKS_REMOVE_CURRENT_DESK,
     IDS_ASH_ACCELERATOR_ACTION_DESKS_REMOVE_CURRENT_DESK},
    {DESKS_ACTIVATE_0, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_ACTIVATE_1, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_ACTIVATE_2, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_ACTIVATE_3, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_ACTIVATE_4, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_ACTIVATE_5, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_ACTIVATE_6, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_ACTIVATE_7, IDS_ASH_ACCELERATOR_ACTION_DESKS_ACTIVATE},
    {DESKS_TOGGLE_ASSIGN_TO_ALL_DESKS,
     IDS_ASH_ACCELERATOR_ACTIONDESKS_TOGGLE_ASSIGN_TO_ALL_DESKS},
    {DISABLE_CAPS_LOCK, IDS_ASH_ACCELERATOR_ACTION_DISABLE_CAPS_LOCK},
    {EXIT, IDS_ASH_ACCELERATOR_ACTION_EXIT},
    {FOCUS_CAMERA_PREVIEW, IDS_ASH_ACCELERATOR_ACTION_FOCUS_CAMERA_PREVIEW},
    {FOCUS_NEXT_PANE, IDS_ASH_ACCELERATOR_ACTION_FOCUS_NEXT_PANE},
    {FOCUS_PREVIOUS_PANE, IDS_ASH_ACCELERATOR_ACTION_FOCUS_PREVIOUS_PANE},
    {FOCUS_SHELF, IDS_ASH_ACCELERATOR_ACTION_FOCUS_SHELF},
    {FOCUS_PIP, IDS_ASH_ACCELERATOR_ACTION_FOCUS_PIP},
    {KEYBOARD_BACKLIGHT_TOGGLE,
     IDS_ASH_ACCELERATOR_ACTION_KEYBOARD_BACKLIGHT_TOGGLE},
    {KEYBOARD_BRIGHTNESS_DOWN,
     IDS_ASH_ACCELERATOR_ACTION_KEYBOARD_BRIGHTNESS_DOWN},
    {KEYBOARD_BRIGHTNESS_UP, IDS_ASH_ACCELERATOR_ACTION_KEYBOARD_BRIGHTNESS_UP},
    {LAUNCH_APP_0, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_APP_1, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_APP_2, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_APP_3, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_APP_4, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_APP_5, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_APP_6, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_APP_7, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_APP},
    {LAUNCH_LAST_APP, IDS_ASH_ACCELERATOR_ACTION_LAUNCH_LAST_APP},
    {LOCK_PRESSED, IDS_ASH_ACCELERATOR_ACTION_LOCK_PRESSED},
    {LOCK_RELEASED, IDS_ASH_ACCELERATOR_ACTION_LOCK_RELEASED},
    {LOCK_SCREEN, IDS_ASH_ACCELERATOR_ACTION_LOCK_SCREEN},
    {MAGNIFIER_ZOOM_IN, IDS_ASH_ACCELERATOR_ACTION_MAGNIFIER_ZOOM_IN},
    {MAGNIFIER_ZOOM_OUT, IDS_ASH_ACCELERATOR_ACTION_MAGNIFIER_ZOOM_OUT},
    {MEDIA_FAST_FORWARD, IDS_ASH_ACCELERATOR_ACTION_MEDIA_FAST_FORWARD},
    {MEDIA_NEXT_TRACK, IDS_ASH_ACCELERATOR_ACTION_MEDIA_NEXT_TRACK},
    {MEDIA_PAUSE, IDS_ASH_ACCELERATOR_ACTION_MEDIA_PAUSE},
    {MEDIA_PLAY, IDS_ASH_ACCELERATOR_ACTION_MEDIA_PLAY},
    {MEDIA_PLAY_PAUSE, IDS_ASH_ACCELERATOR_ACTION_MEDIA_PLAY_PAUSE},
    {MEDIA_PREV_TRACK, IDS_ASH_ACCELERATOR_ACTION_MEDIA_PREV_TRACK},
    {MEDIA_REWIND, IDS_ASH_ACCELERATOR_ACTION_MEDIA_REWIND},
    {MEDIA_STOP, IDS_ASH_ACCELERATOR_ACTION_MEDIA_STOP},
    {MICROPHONE_MUTE_TOGGLE, IDS_ASH_ACCELERATOR_ACTION_MICROPHONE_MUTE_TOGGLE},
    {MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS,
     IDS_ASH_ACCELERATOR_ACTION_MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS},
    {NEW_INCOGNITO_WINDOW, IDS_ASH_ACCELERATOR_ACTION_NEW_INCOGNITO_WINDOW},
    {NEW_TAB, IDS_ASH_ACCELERATOR_ACTION_NEW_TAB},
    {NEW_WINDOW, IDS_ASH_ACCELERATOR_ACTION_NEW_WINDOW},
    {OPEN_CALCULATOR, IDS_ASH_ACCELERATOR_ACTION_OPEN_CALCULATOR},
    {OPEN_CROSH, IDS_ASH_ACCELERATOR_ACTION_OPEN_CROSH},
    {OPEN_DIAGNOSTICS, IDS_ASH_ACCELERATOR_ACTION_OPEN_DIAGNOSTICS},
    {OPEN_FEEDBACK_PAGE, IDS_ASH_ACCELERATOR_ACTION_OPEN_FEEDBACK_PAGE},
    {OPEN_FILE_MANAGER, IDS_ASH_ACCELERATOR_ACTION_OPEN_FILE_MANAGER},
    {OPEN_GET_HELP, IDS_ASH_ACCELERATOR_ACTION_OPEN_GET_HELP},
    {POWER_PRESSED, IDS_ASH_ACCELERATOR_ACTION_POWER_PRESSED},
    {POWER_RELEASED, IDS_ASH_ACCELERATOR_ACTION_POWER_RELEASED},
    {PRINT_UI_HIERARCHIES, IDS_ASH_ACCELERATOR_ACTION_PRINT_UI_HIERARCHIES},
    {PRIVACY_SCREEN_TOGGLE, IDS_ASH_ACCELERATOR_ACTION_PRIVACY_SCREEN_TOGGLE},
    {RESTORE_TAB, IDS_ASH_ACCELERATOR_ACTION_RESTORE_TAB},
    {ROTATE_SCREEN, IDS_ASH_ACCELERATOR_ACTION_ROTATE_SCREEN},
    {ROTATE_WINDOW, IDS_ASH_ACCELERATOR_ACTION_ROTATE_WINDOW},
    {SCALE_UI_DOWN, IDS_ASH_ACCELERATOR_ACTION_SCALE_UI_DOWN},
    {SCALE_UI_RESET, IDS_ASH_ACCELERATOR_ACTION_SCALE_UI_RESET},
    {SCALE_UI_UP, IDS_ASH_ACCELERATOR_ACTION_SCALE_UI_UP},
    {SHOW_EMOJI_PICKER, IDS_ASH_ACCELERATOR_ACTION_SHOW_EMOJI_PICKER},
    {TOGGLE_IME_MENU_BUBBLE, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_IME_MENU_BUBBLE},
    {SHOW_SHORTCUT_VIEWER, IDS_ASH_ACCELERATOR_ACTION_SHOW_SHORTCUT_VIEWER},
    {SHOW_STYLUS_TOOLS, IDS_ASH_ACCELERATOR_ACTION_SHOW_STYLUS_TOOLS},
    {SHOW_TASK_MANAGER, IDS_ASH_ACCELERATOR_ACTION_SHOW_TASK_MANAGER},
    {START_AMBIENT_MODE, IDS_ASH_ACCELERATOR_ACTION_START_AMBIENT_MODE},
    {START_ASSISTANT, IDS_ASH_ACCELERATOR_ACTION_START_ASSISTANT},
    {SUSPEND, IDS_ASH_ACCELERATOR_ACTION_SUSPEND},
    {SWAP_PRIMARY_DISPLAY, IDS_ASH_ACCELERATOR_ACTION_SWAP_PRIMARY_DISPLAY},
    {SWITCH_IME, IDS_ASH_ACCELERATOR_ACTION_SWITCH_IME},
    {SWITCH_TO_LAST_USED_IME,
     IDS_ASH_ACCELERATOR_ACTION_SWITCH_TO_LAST_USED_IME},
    {SWITCH_TO_NEXT_IME, IDS_ASH_ACCELERATOR_ACTION_SWITCH_TO_NEXT_IME},
    {SWITCH_TO_NEXT_USER, IDS_ASH_ACCELERATOR_ACTION_SWITCH_TO_NEXT_USER},
    {SWITCH_TO_PREVIOUS_USER,
     IDS_ASH_ACCELERATOR_ACTION_SWITCH_TO_PREVIOUS_USER},
    {TAKE_PARTIAL_SCREENSHOT,
     IDS_ASH_ACCELERATOR_ACTION_TAKE_PARTIAL_SCREENSHOT},
    {TAKE_SCREENSHOT, IDS_ASH_ACCELERATOR_ACTION_TAKE_SCREENSHOT},
    {TAKE_WINDOW_SCREENSHOT, IDS_ASH_ACCELERATOR_ACTION_TAKE_WINDOW_SCREENSHOT},
    {TOGGLE_APP_LIST, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_APP_LIST},
    {TOGGLE_CALENDAR, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_CALENDAR},
    {TOGGLE_CAPS_LOCK, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_CAPS_LOCK},
    {TOGGLE_CLIPBOARD_HISTORY,
     IDS_ASH_ACCELERATOR_ACTION_TOGGLE_CLIPBOARD_HISTORY},
    {TOGGLE_DICTATION, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_DICTATION},
    {TOGGLE_DOCKED_MAGNIFIER,
     IDS_ASH_ACCELERATOR_ACTION_TOGGLE_DOCKED_MAGNIFIER},
    {TOGGLE_FLOATING, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_FLOATING},
    {TOGGLE_FULLSCREEN, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_FULLSCREEN},
    {TOGGLE_FULLSCREEN_MAGNIFIER,
     IDS_ASH_ACCELERATOR_ACTION_TOGGLE_FULLSCREEN_MAGNIFIER},
    {TOGGLE_HIGH_CONTRAST, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_HIGH_CONTRAST},
    {TOGGLE_MAXIMIZED, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_MAXIMIZED},
    {TOGGLE_MESSAGE_CENTER_BUBBLE,
     IDS_ASH_ACCELERATOR_ACTION_TOGGLE_MESSAGE_CENTER_BUBBLE},
    {TOGGLE_MIRROR_MODE, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_MIRROR_MODE},
    {TOGGLE_OVERVIEW, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_OVERVIEW},
    {TOGGLE_PROJECTOR_MARKER,
     IDS_ASH_ACCELERATOR_ACTION_TOGGLE_PROJECTOR_MARKER},
    {TOGGLE_RESIZE_LOCK_MENU,
     IDS_ASH_ACCELERATOR_ACTION_TOGGLE_RESIZE_LOCK_MENU},
    {TOGGLE_SPOKEN_FEEDBACK, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_SPOKEN_FEEDBACK},
    {TOGGLE_SYSTEM_TRAY_BUBBLE,
     IDS_ASH_ACCELERATOR_ACTION_TOGGLE_SYSTEM_TRAY_BUBBLE},
    {TOGGLE_WIFI, IDS_ASH_ACCELERATOR_ACTION_TOGGLE_WIFI},
    {TOUCH_HUD_CLEAR, IDS_ASH_ACCELERATOR_ACTION_TOUCH_HUD_CLEAR},
    {TOUCH_HUD_MODE_CHANGE, IDS_ASH_ACCELERATOR_ACTION_TOUCH_HUD_MODE_CHANGE},
    {UNPIN, IDS_ASH_ACCELERATOR_ACTION_UNPIN},
    {VOLUME_DOWN, IDS_ASH_ACCELERATOR_ACTION_VOLUME_DOWN},
    {VOLUME_MUTE, IDS_ASH_ACCELERATOR_ACTION_VOLUME_MUTE},
    {VOLUME_UP, IDS_ASH_ACCELERATOR_ACTION_VOLUME_UP},
    {WINDOW_CYCLE_SNAP_LEFT, IDS_ASH_ACCELERATOR_ACTION_WINDOW_CYCLE_SNAP_LEFT},
    {WINDOW_CYCLE_SNAP_RIGHT,
     IDS_ASH_ACCELERATOR_ACTION_WINDOW_CYCLE_SNAP_RIGHT},
    {WINDOW_MINIMIZE, IDS_ASH_ACCELERATOR_ACTION_WINDOW_MINIMIZE},
    {MINIMIZE_TOP_WINDOW_ON_BACK,
     IDS_ASH_ACCELERATOR_ACTION_MINIMIZE_TOP_WINDOW_ON_BACK},
    {DEBUG_DUMP_CALENDAR_MODEL,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_DUMP_CALENDAR_MODEL},
    {DEBUG_KEYBOARD_BACKLIGHT_TOGGLE,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_KEYBOARD_BACKLIGHT_TOGGLE},
    {DEBUG_MICROPHONE_MUTE_TOGGLE,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_MICROPHONE_MUTE_TOGGLE},
    {DEBUG_PRINT_LAYER_HIERARCHY,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_PRINT_LAYER_HIERARCHY},
    {DEBUG_PRINT_VIEW_HIERARCHY,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_PRINT_VIEW_HIERARCHY},
    {DEBUG_PRINT_WINDOW_HIERARCHY,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_PRINT_WINDOW_HIERARCHY},
    {DEBUG_SHOW_TOAST, IDS_ASH_ACCELERATOR_ACTION_DEBUG_SHOW_TOAST},
    {DEBUG_SYSTEM_UI_STYLE_VIEWER,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_SYSTEM_UI_STYLE_VIEWER},
    {DEBUG_TUCK_FLOATED_WINDOW_LEFT,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TUCK_FLOATED_WINDOW_LEFT},
    {DEBUG_TUCK_FLOATED_WINDOW_RIGHT,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TUCK_FLOATED_WINDOW_RIGHT},
    {DEBUG_TOGGLE_DARK_MODE, IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_DARK_MODE},
    {DEBUG_TOGGLE_DYNAMIC_COLOR,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_DYNAMIC_COLOR},
    {DEBUG_TOGGLE_GLANCEABLES,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_GLANCEABLES},
    {DEBUG_TOGGLE_SHOW_DEBUG_BORDERS,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_SHOW_DEBUG_BORDERS},
    {DEBUG_TOGGLE_SHOW_FPS_COUNTER,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_SHOW_FPS_COUNTER},
    {DEBUG_TOGGLE_SHOW_PAINT_RECTS,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_SHOW_PAINT_RECTS},
    {DEBUG_TOGGLE_TOUCH_PAD, IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_TOUCH_PAD},
    {DEBUG_TOGGLE_TOUCH_SCREEN,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_TOUCH_SCREEN},
    {DEBUG_TOGGLE_TABLET_MODE,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_TABLET_MODE},
    {DEBUG_TOGGLE_VIDEO_CONFERENCE_CAMERA_TRAY_ICON,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_VIDEO_CONFERENCE_CAMERA_TRAY_ICON},
    {DEBUG_TOGGLE_WALLPAPER_MODE,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_WALLPAPER_MODE},
    {DEBUG_TRIGGER_CRASH, IDS_ASH_ACCELERATOR_ACTION_DEBUG_TRIGGER_CRASH},
    {DEBUG_TOGGLE_HUD_DISPLAY,
     IDS_ASH_ACCELERATOR_ACTION_DEBUG_TOGGLE_HUD_DISPLAY},
    {DEV_ADD_REMOVE_DISPLAY, IDS_ASH_ACCELERATOR_ACTION_DEV_ADD_REMOVE_DISPLAY},
    {DEV_TOGGLE_APP_LIST, IDS_ASH_ACCELERATOR_ACTION_DEV_TOGGLE_APP_LIST},
    {DEV_TOGGLE_UNIFIED_DESKTOP,
     IDS_ASH_ACCELERATOR_ACTION_DEV_TOGGLE_UNIFIED_DESKTOP},
});

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

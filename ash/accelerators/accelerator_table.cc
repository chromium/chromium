// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"

#include "ash/public/cpp/accelerators.h"
#include "ash/strings/grit/ash_strings.h"
#include "build/branding_buildflags.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// Instructions for how to deprecate and replace an Accelerator:
//
// 1- Replace the old deprecated accelerator from the above list with the new
//    accelerator that will take its place.
// 2- Add an entry for it in the following |kDeprecatedAccelerators| list.
// 3- Add another entry in the |kDeprecatedAcceleratorsData|.
// 4- That entry should contain the following:
//    - The action that the deprecated accelerator maps to.
//    - Define a histogram for this action in |histograms.xml| in the form
//      "Ash.Accelerators.Deprecated.{ActionName}" and include the name of this
//      histogram in this entry. This name will be used as the ID of the
//      notification to be shown to the user. This is to prevent duplication of
//      same notification.
//    - The ID of the localized notification message to give the users telling
//      them about the deprecation (Add one in |ash_strings.grd|. Search for
//      the comment <!-- Deprecated Accelerators Messages -->).
//    - The IDs of the localized old and new shortcut text to be used to fill
//      the notification text. Also found in |ash_strings.grd|.
//    - {true or false} whether the deprecated accelerator is still enabled (we
//      don't disable a deprecated accelerator abruptly).
// 5- Don't forget to update the keyboard_shortcut_viewer_metadata.cc and
//    shortcut_viewer_strings.grdp.
const AcceleratorData kDeprecatedAccelerators[] = {
    {true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, SHOW_TASK_MANAGER},

    // Deprecated in M59.
    {true, ui::VKEY_K, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     TOGGLE_IME_MENU_BUBBLE}};

const size_t kDeprecatedAcceleratorsLength = std::size(kDeprecatedAccelerators);

const DeprecatedAcceleratorData kDeprecatedAcceleratorsData[] = {
    // The action for the old shortcut was stopped in M92. Delete
    // completely in M94.
    {SHOW_TASK_MANAGER, "Ash.Accelerators.Deprecated.ShowTaskManager",
     IDS_DEPRECATED_SHOW_TASK_MANAGER_MSG, IDS_SHORTCUT_TASK_MANAGER_OLD,
     IDS_SHORTCUT_TASK_MANAGER_NEW, false},
    // The action for the old shortcut was stopped in M92. Delete
    // completely in M94.
    {TOGGLE_IME_MENU_BUBBLE, "Ash.Accelerators.Deprecated.ShowImeMenuBubble",
     IDS_DEPRECATED_SHOW_IME_BUBBLE_MSG, IDS_SHORTCUT_IME_BUBBLE_OLD,
     IDS_SHORTCUT_IME_BUBBLE_NEW, false}};

const size_t kDeprecatedAcceleratorsDataLength =
    std::size(kDeprecatedAcceleratorsData);

static_assert(kDeprecatedAcceleratorsLength ==
                  kDeprecatedAcceleratorsDataLength,
              "Deprecated accelerator tables must be kept in sync");

const AcceleratorData kDebugAcceleratorData[] = {
    {true, ui::VKEY_N, kDebugModifier, TOGGLE_WIFI},
    {true, ui::VKEY_X, kDebugModifier, DEBUG_KEYBOARD_BACKLIGHT_TOGGLE},
    {true, ui::VKEY_M, kDebugModifier, DEBUG_MICROPHONE_MUTE_TOGGLE},
    {true, ui::VKEY_O, kDebugModifier, DEBUG_SHOW_TOAST},
    {true, ui::VKEY_Z, kDebugModifier, DEBUG_SYSTEM_UI_STYLE_VIEWER},
    {true, ui::VKEY_P, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DEBUG_TOGGLE_TOUCH_PAD},
    {true, ui::VKEY_T, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DEBUG_TOGGLE_TOUCH_SCREEN},
    {true, ui::VKEY_T, kDebugModifier, DEBUG_TOGGLE_TABLET_MODE},
    {true, ui::VKEY_A, kDebugModifier,
     DEBUG_TOGGLE_VIDEO_CONFERENCE_CAMERA_TRAY_ICON},
    {true, ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     DEBUG_TOGGLE_WALLPAPER_MODE},
    {true, ui::VKEY_L, kDebugModifier, DEBUG_PRINT_LAYER_HIERARCHY},
    {true, ui::VKEY_V, kDebugModifier, DEBUG_PRINT_VIEW_HIERARCHY},
    {true, ui::VKEY_W, kDebugModifier, DEBUG_PRINT_WINDOW_HIERARCHY},
    {true, ui::VKEY_9, kDebugModifier, DEBUG_TOGGLE_GLANCEABLES},
    {true, ui::VKEY_B, kDebugModifier, DEBUG_TOGGLE_SHOW_DEBUG_BORDERS},
    {true, ui::VKEY_F, kDebugModifier, DEBUG_TOGGLE_SHOW_FPS_COUNTER},
    {true, ui::VKEY_P, kDebugModifier, DEBUG_TOGGLE_SHOW_PAINT_RECTS},
    {true, ui::VKEY_K, kDebugModifier, DEBUG_TRIGGER_CRASH},
    {true, ui::VKEY_G, kDebugModifier, DEBUG_TOGGLE_HUD_DISPLAY},
    {true, ui::VKEY_D, kDebugModifier, DEBUG_TOGGLE_DARK_MODE},
    {true, ui::VKEY_OEM_4, kDebugModifier, DEBUG_TUCK_FLOATED_WINDOW_LEFT},
    {true, ui::VKEY_OEM_6, kDebugModifier, DEBUG_TUCK_FLOATED_WINDOW_RIGHT},
    {true, ui::VKEY_Y, kDebugModifier, DEBUG_TOGGLE_DYNAMIC_COLOR},
};

const size_t kDebugAcceleratorDataLength = std::size(kDebugAcceleratorData);

const AcceleratorData kDeveloperAcceleratorData[] = {
    // Extra shortcut for debug build to control magnifier on Linux desktop.
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN, MAGNIFIER_ZOOM_OUT},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN, MAGNIFIER_ZOOM_IN},
    // Extra shortcuts to lock the screen on Linux desktop.
    {true, ui::VKEY_L, ui::EF_ALT_DOWN, LOCK_PRESSED},
    {false, ui::VKEY_L, ui::EF_ALT_DOWN, LOCK_RELEASED},
    {true, ui::VKEY_P, ui::EF_ALT_DOWN, POWER_PRESSED},
    {false, ui::VKEY_P, ui::EF_ALT_DOWN, POWER_RELEASED},
    {true, ui::VKEY_POWER, ui::EF_SHIFT_DOWN, LOCK_PRESSED},
    {false, ui::VKEY_POWER, ui::EF_SHIFT_DOWN, LOCK_RELEASED},
    {true, ui::VKEY_D, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     DEV_ADD_REMOVE_DISPLAY},
    {true, ui::VKEY_U, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     DEV_TOGGLE_UNIFIED_DESKTOP},
    {true, ui::VKEY_M, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     TOGGLE_MIRROR_MODE},
    {true, ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, TOGGLE_WIFI},
    // Extra shortcut for display swapping as Alt-F4 is taken on Linux desktop.
    {true, ui::VKEY_S, kDebugModifier, SWAP_PRIMARY_DISPLAY},
    // Extra shortcut to rotate/scale up/down the screen on Linux desktop.
    {true, ui::VKEY_R, kDebugModifier, ROTATE_SCREEN},
    // For testing on systems where Alt-Tab is already mapped.
    {true, ui::VKEY_W, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU},
    {true, ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, CYCLE_BACKWARD_MRU},
    {true, ui::VKEY_F, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     TOGGLE_FULLSCREEN},
    // TODO(wutao): Get a shortcut for the Ambient mode.
    {true, ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     START_AMBIENT_MODE},
    // For testing on Linux desktop where it's hard to rebind the caps lock key.
    {true, ui::VKEY_A, ui::EF_ALT_DOWN, DEV_TOGGLE_APP_LIST},
};

const size_t kDeveloperAcceleratorDataLength =
    std::size(kDeveloperAcceleratorData);

const AcceleratorAction kPreferredActions[] = {
    // Window cycling accelerators.
    CYCLE_BACKWARD_MRU,               // Shift+Alt+Tab
    CYCLE_FORWARD_MRU,                // Alt+Tab
    CYCLE_SAME_APP_WINDOWS_BACKWARD,  // Shift+Alt+Backtick
    CYCLE_SAME_APP_WINDOWS_FORWARD,   // Alt+Backtick
};

const size_t kPreferredActionsLength = std::size(kPreferredActions);

const AcceleratorAction kReservedActions[] = {
    POWER_PRESSED, POWER_RELEASED, LOCK_PRESSED, LOCK_RELEASED, SUSPEND,
};

const size_t kReservedActionsLength = std::size(kReservedActions);

const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[] = {
    BRIGHTNESS_DOWN,
    BRIGHTNESS_UP,
    DEBUG_PRINT_LAYER_HIERARCHY,
    DEBUG_PRINT_VIEW_HIERARCHY,
    DEBUG_PRINT_WINDOW_HIERARCHY,
    DEBUG_TOGGLE_HUD_DISPLAY,
    DEBUG_TOGGLE_TOUCH_PAD,
    DEBUG_TOGGLE_TOUCH_SCREEN,
    DEBUG_TOGGLE_TABLET_MODE,
    DEV_ADD_REMOVE_DISPLAY,
    DISABLE_CAPS_LOCK,
    KEYBOARD_BACKLIGHT_TOGGLE,
    KEYBOARD_BRIGHTNESS_DOWN,
    KEYBOARD_BRIGHTNESS_UP,
    MAGNIFIER_ZOOM_IN,   // Control+F7
    MAGNIFIER_ZOOM_OUT,  // Control+F6
    MEDIA_FAST_FORWARD,
    MEDIA_NEXT_TRACK,
    MEDIA_PAUSE,
    MEDIA_PLAY,
    MEDIA_PLAY_PAUSE,
    MEDIA_PREV_TRACK,
    MEDIA_REWIND,
    MEDIA_STOP,
    MICROPHONE_MUTE_TOGGLE,
    PRIVACY_SCREEN_TOGGLE,
    PRINT_UI_HIERARCHIES,
    ROTATE_SCREEN,
    SCALE_UI_DOWN,
    SCALE_UI_RESET,
    SCALE_UI_UP,
    TOGGLE_IME_MENU_BUBBLE,
    START_AMBIENT_MODE,
    SWITCH_TO_LAST_USED_IME,
    SWITCH_TO_NEXT_IME,
    TAKE_SCREENSHOT,
    TOGGLE_CALENDAR,
    TOGGLE_CAPS_LOCK,
    TOGGLE_DICTATION,
    TOGGLE_DOCKED_MAGNIFIER,
    TOGGLE_FULLSCREEN_MAGNIFIER,
    TOGGLE_HIGH_CONTRAST,
    TOGGLE_MIRROR_MODE,
    TOGGLE_SPOKEN_FEEDBACK,
    TOGGLE_SYSTEM_TRAY_BUBBLE,
    TOGGLE_WIFI,
    TOUCH_HUD_CLEAR,
    VOLUME_DOWN,
    VOLUME_MUTE,
    VOLUME_UP,
#if !defined(NDEBUG)
    POWER_PRESSED,
    POWER_RELEASED,
#endif  // !defined(NDEBUG)
};

const size_t kActionsAllowedAtLoginOrLockScreenLength =
    std::size(kActionsAllowedAtLoginOrLockScreen);

const AcceleratorAction kActionsAllowedAtLockScreen[] = {
    EXIT,
    SUSPEND,
};

const size_t kActionsAllowedAtLockScreenLength =
    std::size(kActionsAllowedAtLockScreen);

const AcceleratorAction kActionsAllowedAtPowerMenu[] = {
    BRIGHTNESS_DOWN, BRIGHTNESS_UP, VOLUME_DOWN, VOLUME_UP, VOLUME_MUTE,
};

const size_t kActionsAllowedAtPowerMenuLength =
    std::size(kActionsAllowedAtPowerMenu);

const AcceleratorAction kActionsAllowedAtModalWindow[] = {
    BRIGHTNESS_DOWN,
    BRIGHTNESS_UP,
    DEBUG_KEYBOARD_BACKLIGHT_TOGGLE,
    DEBUG_MICROPHONE_MUTE_TOGGLE,
    DEBUG_TOGGLE_TOUCH_PAD,
    DEBUG_TOGGLE_TOUCH_SCREEN,
    DEV_ADD_REMOVE_DISPLAY,
    DISABLE_CAPS_LOCK,
    EXIT,
    KEYBOARD_BACKLIGHT_TOGGLE,
    KEYBOARD_BRIGHTNESS_DOWN,
    KEYBOARD_BRIGHTNESS_UP,
    LOCK_SCREEN,
    MAGNIFIER_ZOOM_IN,
    MAGNIFIER_ZOOM_OUT,
    MEDIA_FAST_FORWARD,
    MEDIA_NEXT_TRACK,
    MEDIA_PAUSE,
    MEDIA_PLAY,
    MEDIA_PLAY_PAUSE,
    MEDIA_PREV_TRACK,
    MEDIA_REWIND,
    MEDIA_STOP,
    MICROPHONE_MUTE_TOGGLE,
    OPEN_FEEDBACK_PAGE,
    POWER_PRESSED,
    POWER_RELEASED,
    PRINT_UI_HIERARCHIES,
    PRIVACY_SCREEN_TOGGLE,
    ROTATE_SCREEN,
    SCALE_UI_DOWN,
    SCALE_UI_RESET,
    SCALE_UI_UP,
    TOGGLE_IME_MENU_BUBBLE,
    SHOW_SHORTCUT_VIEWER,
    START_AMBIENT_MODE,
    SUSPEND,
    SWAP_PRIMARY_DISPLAY,
    SWITCH_TO_LAST_USED_IME,
    SWITCH_TO_NEXT_IME,
    TAKE_PARTIAL_SCREENSHOT,
    TAKE_SCREENSHOT,
    TAKE_WINDOW_SCREENSHOT,
    TOGGLE_CAPS_LOCK,
    TOGGLE_DICTATION,
    TOGGLE_DOCKED_MAGNIFIER,
    TOGGLE_FULLSCREEN_MAGNIFIER,
    TOGGLE_HIGH_CONTRAST,
    TOGGLE_MIRROR_MODE,
    TOGGLE_SPOKEN_FEEDBACK,
    TOGGLE_WIFI,
    VOLUME_DOWN,
    VOLUME_MUTE,
    VOLUME_UP,
};

const size_t kActionsAllowedAtModalWindowLength =
    std::size(kActionsAllowedAtModalWindow);

const AcceleratorAction kRepeatableActions[] = {
    BRIGHTNESS_DOWN,
    BRIGHTNESS_UP,
    FOCUS_NEXT_PANE,
    FOCUS_PREVIOUS_PANE,
    KEYBOARD_BRIGHTNESS_DOWN,
    KEYBOARD_BRIGHTNESS_UP,
    MAGNIFIER_ZOOM_IN,
    MAGNIFIER_ZOOM_OUT,
    MEDIA_FAST_FORWARD,
    MEDIA_NEXT_TRACK,
    MEDIA_PREV_TRACK,
    MEDIA_REWIND,
    RESTORE_TAB,
    VOLUME_DOWN,
    VOLUME_UP,
};

const size_t kRepeatableActionsLength = std::size(kRepeatableActions);

const AcceleratorAction kActionsAllowedInAppModeOrPinnedMode[] = {
    BRIGHTNESS_DOWN,
    BRIGHTNESS_UP,
    DEBUG_KEYBOARD_BACKLIGHT_TOGGLE,
    DEBUG_MICROPHONE_MUTE_TOGGLE,
    DEBUG_PRINT_LAYER_HIERARCHY,
    DEBUG_PRINT_VIEW_HIERARCHY,
    DEBUG_PRINT_WINDOW_HIERARCHY,
    DEBUG_TOGGLE_TOUCH_PAD,
    DEBUG_TOGGLE_TOUCH_SCREEN,
    DEV_ADD_REMOVE_DISPLAY,
    DISABLE_CAPS_LOCK,
    KEYBOARD_BACKLIGHT_TOGGLE,
    KEYBOARD_BRIGHTNESS_DOWN,
    KEYBOARD_BRIGHTNESS_UP,
    MAGNIFIER_ZOOM_IN,   // Control+F7
    MAGNIFIER_ZOOM_OUT,  // Control+F6
    MEDIA_FAST_FORWARD,
    MEDIA_NEXT_TRACK,
    MEDIA_PAUSE,
    MEDIA_PLAY,
    MEDIA_PLAY_PAUSE,
    MEDIA_PREV_TRACK,
    MEDIA_REWIND,
    MEDIA_STOP,
    MICROPHONE_MUTE_TOGGLE,
    PASTE_CLIPBOARD_HISTORY_PLAIN_TEXT,
    POWER_PRESSED,
    POWER_RELEASED,
    PRINT_UI_HIERARCHIES,
    PRIVACY_SCREEN_TOGGLE,
    ROTATE_SCREEN,
    SCALE_UI_DOWN,
    SCALE_UI_RESET,
    SCALE_UI_UP,
    SWAP_PRIMARY_DISPLAY,
    SWITCH_TO_LAST_USED_IME,
    SWITCH_TO_NEXT_IME,
    TOGGLE_CAPS_LOCK,
    TOGGLE_CLIPBOARD_HISTORY,
    TOGGLE_DICTATION,
    TOGGLE_DOCKED_MAGNIFIER,
    TOGGLE_FULLSCREEN_MAGNIFIER,
    TOGGLE_HIGH_CONTRAST,
    TOGGLE_MIRROR_MODE,
    TOGGLE_SPOKEN_FEEDBACK,
    TOGGLE_WIFI,
    TOUCH_HUD_CLEAR,
    VOLUME_DOWN,
    VOLUME_MUTE,
    VOLUME_UP,
};

const size_t kActionsAllowedInAppModeOrPinnedModeLength =
    std::size(kActionsAllowedInAppModeOrPinnedMode);

const AcceleratorAction kActionsAllowedInPinnedMode[] = {
    LOCK_SCREEN,
    SUSPEND,
    TAKE_PARTIAL_SCREENSHOT,
    TAKE_SCREENSHOT,
    TAKE_WINDOW_SCREENSHOT,
    UNPIN,
};

const size_t kActionsAllowedInPinnedModeLength =
    std::size(kActionsAllowedInPinnedMode);

const AcceleratorAction kActionsAllowedInAppMode[] = {
    FOCUS_SHELF,
};

const size_t kActionsAllowedInAppModeLength =
    std::size(kActionsAllowedInAppMode);

const AcceleratorAction kActionsNeedingWindow[] = {
    // clang-format off
    DESKS_MOVE_ACTIVE_ITEM_LEFT,
    DESKS_MOVE_ACTIVE_ITEM_RIGHT,
    DESKS_TOGGLE_ASSIGN_TO_ALL_DESKS,
    MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS,
    ROTATE_WINDOW,
    TOGGLE_FLOATING,
    TOGGLE_FULLSCREEN,
    TOGGLE_MAXIMIZED,
    WINDOW_CYCLE_SNAP_LEFT,
    WINDOW_CYCLE_SNAP_RIGHT,
    WINDOW_MINIMIZE,
    // clang-format on
};

const size_t kActionsNeedingWindowLength = std::size(kActionsNeedingWindow);

const AcceleratorAction kActionsKeepingMenuOpen[] = {
    BRIGHTNESS_DOWN,
    BRIGHTNESS_UP,
    DEBUG_KEYBOARD_BACKLIGHT_TOGGLE,
    DEBUG_MICROPHONE_MUTE_TOGGLE,
    DEBUG_TOGGLE_TOUCH_PAD,
    DEBUG_TOGGLE_TOUCH_SCREEN,
    // Keep the menu open when switching desks. The desk activation code will
    // close the menu without animation manually. Otherwise, the menu will fade
    // out and a trace will be visible while switching desks.
    DESKS_ACTIVATE_DESK_LEFT,
    DESKS_ACTIVATE_DESK_RIGHT,
    DESKS_NEW_DESK,
    DESKS_REMOVE_CURRENT_DESK,
    DISABLE_CAPS_LOCK,
    KEYBOARD_BACKLIGHT_TOGGLE,
    KEYBOARD_BRIGHTNESS_DOWN,
    KEYBOARD_BRIGHTNESS_UP,
    MEDIA_FAST_FORWARD,
    MEDIA_NEXT_TRACK,
    MEDIA_PAUSE,
    MEDIA_PLAY,
    MEDIA_PLAY_PAUSE,
    MEDIA_PREV_TRACK,
    MEDIA_REWIND,
    MEDIA_STOP,
    MICROPHONE_MUTE_TOGGLE,
    PASTE_CLIPBOARD_HISTORY_PLAIN_TEXT,
    PRINT_UI_HIERARCHIES,
    PRIVACY_SCREEN_TOGGLE,
    SWITCH_TO_LAST_USED_IME,
    SWITCH_TO_NEXT_IME,
    TAKE_PARTIAL_SCREENSHOT,
    TAKE_SCREENSHOT,
    TAKE_WINDOW_SCREENSHOT,
    TOGGLE_APP_LIST,
    TOGGLE_CAPS_LOCK,
    TOGGLE_CLIPBOARD_HISTORY,
    TOGGLE_DICTATION,
    TOGGLE_DOCKED_MAGNIFIER,
    TOGGLE_FULLSCREEN_MAGNIFIER,
    TOGGLE_HIGH_CONTRAST,
    TOGGLE_SPOKEN_FEEDBACK,
    TOGGLE_WIFI,
    VOLUME_DOWN,
    VOLUME_MUTE,
    VOLUME_UP,
};

const size_t kActionsKeepingMenuOpenLength = std::size(kActionsKeepingMenuOpen);

const AcceleratorAction kActionsDuplicatedWithBrowser[] = {
    // clang-format off
    NEW_WINDOW,
    NEW_INCOGNITO_WINDOW,
    RESTORE_TAB,
    NEW_TAB,
// clang-format on

// OPEN_FEEDBACK_PAGE is guarded by the BRANDING macro on defining Browser's
// shortcuts. To follow it, guard it here, too.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    OPEN_FEEDBACK_PAGE,
#endif
};

const size_t kActionsDuplicatedWithBrowserLength =
    std::size(kActionsDuplicatedWithBrowser);

const AcceleratorAction kActionsInterceptableByBrowser[] = {
    SHOW_TASK_MANAGER,
    OPEN_GET_HELP,
    MINIMIZE_TOP_WINDOW_ON_BACK,
};

const size_t kActionsInterceptableByBrowserLength =
    std::size(kActionsInterceptableByBrowser);

}  // namespace ash

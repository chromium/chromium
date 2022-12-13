// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators.h"

#include "base/callback.h"
#include "base/no_destructor.h"

namespace ash {

namespace {

AcceleratorController* g_instance = nullptr;

base::RepeatingClosure* GetVolumeAdjustmentCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return callback.get();
}

}  // namespace

const AcceleratorData kAcceleratorData[] = {
    {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, SWITCH_TO_LAST_USED_IME},
    {false, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN, SWITCH_TO_LAST_USED_IME},
    {true, ui::VKEY_TAB, ui::EF_ALT_DOWN, CYCLE_FORWARD_MRU},
    {true, ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     CYCLE_BACKWARD_MRU},
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE, TOGGLE_OVERVIEW},
    // Historically, the browser search key with and without the shift key can
    // toggle the app list into different open states. Now the two combinations
    // are used to toggle the app list in the same way to keep the behavior
    // consistent.
    {true, ui::VKEY_BROWSER_SEARCH, ui::EF_NONE, TOGGLE_APP_LIST},
    {true, ui::VKEY_BROWSER_SEARCH, ui::EF_SHIFT_DOWN, TOGGLE_APP_LIST},
    {true, ui::VKEY_ALL_APPLICATIONS, ui::EF_NONE, TOGGLE_APP_LIST},
    {true, ui::VKEY_WLAN, ui::EF_NONE, TOGGLE_WIFI},
    {true, ui::VKEY_PRIVACY_SCREEN_TOGGLE, ui::EF_NONE, PRIVACY_SCREEN_TOGGLE},
    {true, ui::VKEY_MICROPHONE_MUTE_TOGGLE, ui::EF_NONE,
     MICROPHONE_MUTE_TOGGLE},
    {true, ui::VKEY_KBD_BACKLIGHT_TOGGLE, ui::EF_NONE,
     KEYBOARD_BACKLIGHT_TOGGLE},
    {true, ui::VKEY_KBD_BRIGHTNESS_DOWN, ui::EF_NONE, KEYBOARD_BRIGHTNESS_DOWN},
    {true, ui::VKEY_KBD_BRIGHTNESS_UP, ui::EF_NONE, KEYBOARD_BRIGHTNESS_UP},
    // Maximize button.
    {true, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN, TOGGLE_MIRROR_MODE},
    {true, ui::VKEY_ZOOM, ui::EF_ALT_DOWN, SWAP_PRIMARY_DISPLAY},
    // Cycle windows button.
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN, TAKE_SCREENSHOT},
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     TAKE_PARTIAL_SCREENSHOT},
    {true, ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN,
     TAKE_WINDOW_SCREENSHOT},
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE, BRIGHTNESS_DOWN},
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_DOWN},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE, BRIGHTNESS_UP},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_ALT_DOWN, KEYBOARD_BRIGHTNESS_UP},
    {true, ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     MAGNIFIER_ZOOM_OUT},
    {true, ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     MAGNIFIER_ZOOM_IN},
    {true, ui::VKEY_L, ui::EF_COMMAND_DOWN, LOCK_SCREEN},
    {true, ui::VKEY_L, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN, SUSPEND},
    // The lock key on Chrome OS keyboards produces F13 scancodes.
    {true, ui::VKEY_F13, ui::EF_NONE, LOCK_PRESSED},
    {false, ui::VKEY_F13, ui::EF_NONE, LOCK_RELEASED},
    // Generic keyboards can use VKEY_SLEEP to mimic ChromeOS keyboard's lock
    // key.
    {true, ui::VKEY_SLEEP, ui::EF_NONE, LOCK_PRESSED},
    {false, ui::VKEY_SLEEP, ui::EF_NONE, LOCK_RELEASED},
    {true, ui::VKEY_POWER, ui::EF_NONE, POWER_PRESSED},
    {false, ui::VKEY_POWER, ui::EF_NONE, POWER_RELEASED},
    {true, ui::VKEY_MEDIA_LAUNCH_APP2, ui::EF_NONE, OPEN_CALCULATOR},
    {true, ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN,
     OPEN_DIAGNOSTICS},
    {true, ui::VKEY_M, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, OPEN_FILE_MANAGER},
    {true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN, OPEN_GET_HELP},
    {true, ui::VKEY_OEM_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     OPEN_GET_HELP},
    {true, ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, OPEN_CROSH},
    {true, ui::VKEY_I, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     TOUCH_HUD_MODE_CHANGE},
    {true, ui::VKEY_I,
     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
     TOUCH_HUD_CLEAR},
    {true, ui::VKEY_H, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     TOGGLE_HIGH_CONTRAST},
    {true, ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     TOGGLE_SPOKEN_FEEDBACK},
    {true, ui::VKEY_D, ui::EF_COMMAND_DOWN, TOGGLE_DICTATION},
    {true, ui::VKEY_DICTATE, ui::EF_NONE, TOGGLE_DICTATION},
    {true, ui::VKEY_OEM_COMMA, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     SWITCH_TO_PREVIOUS_USER},
    {true, ui::VKEY_OEM_PERIOD, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     SWITCH_TO_NEXT_USER},
    // Single shift release turns off caps lock.
    {false, ui::VKEY_LSHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK},
    {false, ui::VKEY_SHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK},
    {false, ui::VKEY_RSHIFT, ui::EF_NONE, DISABLE_CAPS_LOCK},
    {true, ui::VKEY_C, ui::EF_COMMAND_DOWN, TOGGLE_CALENDAR},
    // Accelerators to toggle Caps Lock.
    // The following is triggered when Search is released while Alt is still
    // down. The key_code here is LWIN (for search) and Alt is a modifier.
    {false, ui::VKEY_LWIN, ui::EF_ALT_DOWN, TOGGLE_CAPS_LOCK},
    // The following is triggered when Alt is released while search is still
    // down. The key_code here is MENU (for Alt) and Search is a modifier
    // (EF_COMMAND_DOWN is used for Search as a modifier).
    {false, ui::VKEY_MENU, ui::EF_COMMAND_DOWN, TOGGLE_CAPS_LOCK},
    {true, ui::VKEY_V, ui::EF_COMMAND_DOWN, TOGGLE_CLIPBOARD_HISTORY},
    {true, ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     PASTE_CLIPBOARD_HISTORY_PLAIN_TEXT},
    {true, ui::VKEY_VOLUME_MUTE, ui::EF_NONE, VOLUME_MUTE},
    {true, ui::VKEY_VOLUME_DOWN, ui::EF_NONE, VOLUME_DOWN},
    {true, ui::VKEY_VOLUME_UP, ui::EF_NONE, VOLUME_UP},
    {true, ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN, SHOW_TASK_MANAGER},
    {true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     SWITCH_TO_NEXT_IME},
    {true, ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, OPEN_FEEDBACK_PAGE},
    {true, ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, EXIT},
    {true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     NEW_INCOGNITO_WINDOW},
    {true, ui::VKEY_N, ui::EF_CONTROL_DOWN, NEW_WINDOW},
    {true, ui::VKEY_T, ui::EF_CONTROL_DOWN, NEW_TAB},
    {true, ui::VKEY_NEW, ui::EF_NONE, NEW_TAB},
    {true, ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     SCALE_UI_UP},
    {true, ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     SCALE_UI_DOWN},
    {true, ui::VKEY_0, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, SCALE_UI_RESET},
    {true, ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     ROTATE_SCREEN},
    {true, ui::VKEY_BROWSER_REFRESH,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, ROTATE_WINDOW},
    {true, ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, RESTORE_TAB},
    // This corresponds to the "Print Screen" key.
    {true, ui::VKEY_SNAPSHOT, ui::EF_NONE, TAKE_SCREENSHOT},
    {true, ui::VKEY_SNAPSHOT, ui::EF_ALT_DOWN, TAKE_PARTIAL_SCREENSHOT},
    // On Chrome OS, Search key is mapped to LWIN. The Search key binding should
    // act on release instead of press when using Search as a modifier key for
    // extended keyboard shortcuts.
    {false, ui::VKEY_LWIN, ui::EF_NONE, TOGGLE_APP_LIST},
    {false, ui::VKEY_LWIN, ui::EF_SHIFT_DOWN, TOGGLE_APP_LIST},
    {true, ui::VKEY_ZOOM, ui::EF_NONE, TOGGLE_FULLSCREEN},
    {true, ui::VKEY_ZOOM, ui::EF_SHIFT_DOWN, TOGGLE_FULLSCREEN},
    {true, ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN, UNPIN},
    {true, ui::VKEY_S, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN,
     FOCUS_CAMERA_PREVIEW},
    {true, ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, FOCUS_SHELF},
    {true, ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, FOCUS_PIP},
    {true, ui::VKEY_HELP, ui::EF_NONE, SHOW_SHORTCUT_VIEWER},
    {true, ui::VKEY_OEM_2, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     SHOW_SHORTCUT_VIEWER},
    {true, ui::VKEY_OEM_2,
     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
     SHOW_SHORTCUT_VIEWER},
    {true, ui::VKEY_F14, ui::EF_NONE, SHOW_SHORTCUT_VIEWER},
    {true, ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     TOGGLE_MESSAGE_CENTER_BUBBLE},
    {true, ui::VKEY_P, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN, SHOW_STYLUS_TOOLS},
    {true, ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     TOGGLE_SYSTEM_TRAY_BUBBLE},
    // Until we have unified settings and notifications the "hamburger"
    // key opens quick settings.
    {true, ui::VKEY_SETTINGS, ui::EF_NONE, TOGGLE_SYSTEM_TRAY_BUBBLE},
    {true, ui::VKEY_K, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     TOGGLE_IME_MENU_BUBBLE},
    {true, ui::VKEY_1, ui::EF_ALT_DOWN, LAUNCH_APP_0},
    {true, ui::VKEY_2, ui::EF_ALT_DOWN, LAUNCH_APP_1},
    {true, ui::VKEY_3, ui::EF_ALT_DOWN, LAUNCH_APP_2},
    {true, ui::VKEY_4, ui::EF_ALT_DOWN, LAUNCH_APP_3},
    {true, ui::VKEY_5, ui::EF_ALT_DOWN, LAUNCH_APP_4},
    {true, ui::VKEY_6, ui::EF_ALT_DOWN, LAUNCH_APP_5},
    {true, ui::VKEY_7, ui::EF_ALT_DOWN, LAUNCH_APP_6},
    {true, ui::VKEY_8, ui::EF_ALT_DOWN, LAUNCH_APP_7},
    {true, ui::VKEY_9, ui::EF_ALT_DOWN, LAUNCH_LAST_APP},

    // Window management shortcuts.
    {true, ui::VKEY_OEM_4, ui::EF_ALT_DOWN, WINDOW_CYCLE_SNAP_LEFT},
    {true, ui::VKEY_OEM_6, ui::EF_ALT_DOWN, WINDOW_CYCLE_SNAP_RIGHT},
    {true, ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN, WINDOW_MINIMIZE},
    {true, ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN, TOGGLE_FLOATING},
    {true, ui::VKEY_OEM_PLUS, ui::EF_ALT_DOWN, TOGGLE_MAXIMIZED},
    {true, ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN, FOCUS_NEXT_PANE},
    {true, ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN, FOCUS_PREVIOUS_PANE},
    {true, ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     FOCUS_NEXT_PANE},
    {true, ui::VKEY_BROWSER_BACK, ui::EF_NONE, MINIMIZE_TOP_WINDOW_ON_BACK},

    // Moving active window between displays shortcut.
    {true, ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS},

    // Magnifiers shortcuts.
    {true, ui::VKEY_D, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     TOGGLE_DOCKED_MAGNIFIER},
    {true, ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     TOGGLE_FULLSCREEN_MAGNIFIER},

    // Media Player shortcuts.
    {true, ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE, MEDIA_NEXT_TRACK},
    {true, ui::VKEY_MEDIA_PAUSE, ui::EF_NONE, MEDIA_PAUSE},
    {true, ui::VKEY_MEDIA_PLAY, ui::EF_NONE, MEDIA_PLAY},
    {true, ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE, MEDIA_PLAY_PAUSE},
    {true, ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE, MEDIA_PREV_TRACK},
    {true, ui::VKEY_MEDIA_STOP, ui::EF_NONE, MEDIA_STOP},
    {true, ui::VKEY_OEM_103, ui::EF_NONE, MEDIA_REWIND},
    {true, ui::VKEY_OEM_104, ui::EF_NONE, MEDIA_FAST_FORWARD},

    // Assistant shortcuts.
    {true, ui::VKEY_A, ui::EF_COMMAND_DOWN, START_ASSISTANT},
    {true, ui::VKEY_ASSISTANT, ui::EF_NONE, START_ASSISTANT},

    // IME mode change key.
    {true, ui::VKEY_MODECHANGE, ui::EF_NONE, SWITCH_TO_NEXT_IME},

    // Emoji picker shortcut.
    {true, ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN,
     SHOW_EMOJI_PICKER},
    {true, ui::VKEY_EMOJI_PICKER, ui::EF_NONE, SHOW_EMOJI_PICKER},

    // Debugging shortcuts that need to be available to end-users in
    // release builds.
    {true, ui::VKEY_U, kDebugModifier, PRINT_UI_HIERARCHIES},

    // Virtual Desks shortcuts.
    // Desk activation:
    {true, ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN, DESKS_ACTIVATE_DESK_LEFT},
    {true, ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN, DESKS_ACTIVATE_DESK_RIGHT},
    // Moving windows to desks:
    {true, ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DESKS_MOVE_ACTIVE_ITEM_LEFT},
    {true, ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DESKS_MOVE_ACTIVE_ITEM_RIGHT},
    // TODO(afakhry): Implement moving windows to a desk by its index directly.

    // TODO(yusukes): Handle VKEY_MEDIA_STOP, and VKEY_MEDIA_LAUNCH_MAIL.

    // ARC-specific shortcut.
    {true, ui::VKEY_C, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     TOGGLE_RESIZE_LOCK_MENU},

    // Projector shortcuts.
    {true, ui::VKEY_OEM_3, ui::EF_COMMAND_DOWN, TOGGLE_PROJECTOR_MARKER},
};

const size_t kAcceleratorDataLength = std::size(kAcceleratorData);

const AcceleratorData kDisableWithNewMappingAcceleratorData[] = {
    // Desk creation and removal:
    // Due to https://crbug.com/976487, Search + "=" is always automatically
    // rewritten to F12, and so is Search + "-" to F11. So we had to implement
    // the following two shortcuts as Shift + F11/F12 until we resolve the above
    // issue, accepting the fact that these two shortcuts might sometimes be
    // consumed by apps and pages (since they're not search-based).
    // TODO(afakhry): Change the following to Search+Shift+"+"/"-" once
    // https://crbug.com/976487 is fixed.
    {true, ui::VKEY_F12, ui::EF_SHIFT_DOWN, DESKS_NEW_DESK},
    {true, ui::VKEY_F11, ui::EF_SHIFT_DOWN, DESKS_REMOVE_CURRENT_DESK},
};

const size_t kDisableWithNewMappingAcceleratorDataLength =
    std::size(kDisableWithNewMappingAcceleratorData);

const AcceleratorData kEnableWithNewMappingAcceleratorData[] = {
    // Desk creation and removal:
    {true, ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DESKS_NEW_DESK},
    {true, ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DESKS_REMOVE_CURRENT_DESK},

    // Desk activation:
    {true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     DESKS_ACTIVATE_DESK_LEFT},
    {true, ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
     DESKS_ACTIVATE_DESK_RIGHT},

    // Moving windows to desks:
    {true, ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     DESKS_MOVE_ACTIVE_ITEM_LEFT},
    {true, ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN,
     DESKS_MOVE_ACTIVE_ITEM_RIGHT},

    // Snap
    {true, ui::VKEY_OEM_COMMA,
     ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     WINDOW_CYCLE_SNAP_LEFT},
    {true, ui::VKEY_OEM_PERIOD,
     ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     WINDOW_CYCLE_SNAP_RIGHT},

    // Zoom
    {true, ui::VKEY_UP,
     ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     SCALE_UI_UP},
    {true, ui::VKEY_DOWN,
     ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     SCALE_UI_DOWN},
    {true, ui::VKEY_BACK,
     ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
     SCALE_UI_RESET},

    // Shortcut Viewer
    {true, ui::VKEY_OEM_2, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     SHOW_SHORTCUT_VIEWER},
};

const size_t kEnableWithNewMappingAcceleratorDataLength =
    std::size(kEnableWithNewMappingAcceleratorData);

const AcceleratorData kEnableWithPositionalAcceleratorsData[] = {
    // These are the desk shortcuts as advertised, but previously
    // they were implicitly implemented in terms of F11 and F12
    // due to event rewrites. Since the F-Key rewrites are deprecated
    // these can be implemented based on the keys they actually are.
    //
    // TODO(crbug.com/1179893): Merge these to the main table once
    // IsImprovedKeyboardShortcutsEnabled() is permanently enabled.
    {true, ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DESKS_NEW_DESK},
    {true, ui::VKEY_OEM_MINUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
     DESKS_REMOVE_CURRENT_DESK},
};

const size_t kEnableWithPositionalAcceleratorsDataLength =
    std::size(kEnableWithPositionalAcceleratorsData);

const AcceleratorData
    kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData[] = {
        // Indexed-desk activation:
        {true, ui::VKEY_1, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_0},
        {true, ui::VKEY_2, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_1},
        {true, ui::VKEY_3, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_2},
        {true, ui::VKEY_4, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_3},
        {true, ui::VKEY_5, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_4},
        {true, ui::VKEY_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_5},
        {true, ui::VKEY_7, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_6},
        {true, ui::VKEY_8, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_ACTIVATE_7},
        // Toggle assign to all desks:
        {true, ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN,
         DESKS_TOGGLE_ASSIGN_TO_ALL_DESKS},
};

const size_t kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength =
    std::size(kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData);

const AcceleratorData kEnableWithSameAppWindowCycleAcceleratorData[] = {
    {true, ui::VKEY_OEM_3, ui::EF_ALT_DOWN, CYCLE_SAME_APP_WINDOWS_FORWARD},
    {true, ui::VKEY_OEM_3, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
     CYCLE_SAME_APP_WINDOWS_BACKWARD},
};

const size_t kEnableWithSameAppWindowCycleAcceleratorDataLength =
    std::size(kEnableWithSameAppWindowCycleAcceleratorData);

// static
AcceleratorController* AcceleratorController::Get() {
  return g_instance;
}

// static
void AcceleratorController::SetVolumeAdjustmentSoundCallback(
    const base::RepeatingClosure& closure) {
  DCHECK(GetVolumeAdjustmentCallback()->is_null() || closure.is_null());
  *GetVolumeAdjustmentCallback() = std::move(closure);
}

// static
void AcceleratorController::PlayVolumeAdjustmentSound() {
  if (*GetVolumeAdjustmentCallback())
    GetVolumeAdjustmentCallback()->Run();
}

void AcceleratorController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AcceleratorController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AcceleratorController::AcceleratorController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AcceleratorController::~AcceleratorController() {
  for (auto& obs : observers_)
    obs.OnAcceleratorControllerWillBeDestroyed(this);

  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void AcceleratorController::NotifyActionPerformed(AcceleratorAction action) {
  for (Observer& observer : observers_)
    observer.OnActionPerformed(action);
}

}  // namespace ash

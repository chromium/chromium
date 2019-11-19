// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATORS_H_
#define ASH_PUBLIC_CPP_ACCELERATORS_H_

#include <stddef.h>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
class Accelerator;
class AcceleratorHistory;
}

namespace ash {

// See documentation in ash/accelerators/accelerator_table.h.

enum AcceleratorAction {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_MRU,
  DESKS_ACTIVATE_DESK,
  DESKS_MOVE_ACTIVE_ITEM,
  DESKS_NEW_DESK,
  DESKS_REMOVE_CURRENT_DESK,
  DEV_ADD_REMOVE_DISPLAY,
  DEV_TOGGLE_UNIFIED_DESKTOP,
  DISABLE_CAPS_LOCK,
  EXIT,
  FOCUS_NEXT_PANE,
  FOCUS_PREVIOUS_PANE,
  FOCUS_SHELF,
  FOCUS_PIP,
  KEYBOARD_BRIGHTNESS_DOWN,
  KEYBOARD_BRIGHTNESS_UP,
  LAUNCH_APP_0,
  LAUNCH_APP_1,
  LAUNCH_APP_2,
  LAUNCH_APP_3,
  LAUNCH_APP_4,
  LAUNCH_APP_5,
  LAUNCH_APP_6,
  LAUNCH_APP_7,
  LAUNCH_LAST_APP,
  LOCK_PRESSED,
  LOCK_RELEASED,
  LOCK_SCREEN,
  MAGNIFIER_ZOOM_IN,
  MAGNIFIER_ZOOM_OUT,
  MEDIA_NEXT_TRACK,
  MEDIA_PLAY_PAUSE,
  MEDIA_PREV_TRACK,
  MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS,
  NEW_INCOGNITO_WINDOW,
  NEW_TAB,
  NEW_WINDOW,
  OPEN_CROSH,
  OPEN_FEEDBACK_PAGE,
  OPEN_FILE_MANAGER,
  OPEN_GET_HELP,
  POWER_PRESSED,
  POWER_RELEASED,
  PRINT_UI_HIERARCHIES,
  RESTORE_TAB,
  ROTATE_SCREEN,
  ROTATE_WINDOW,
  SCALE_UI_DOWN,
  SCALE_UI_RESET,
  SCALE_UI_UP,
  SHOW_IME_MENU_BUBBLE,
  SHOW_SHORTCUT_VIEWER,
  SHOW_STYLUS_TOOLS,
  SHOW_TASK_MANAGER,
  START_AMBIENT_MODE,
  START_ASSISTANT,
  SUSPEND,
  SWAP_PRIMARY_DISPLAY,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  SWITCH_TO_LAST_USED_IME,
  SWITCH_TO_NEXT_IME,
  SWITCH_TO_NEXT_USER,
  SWITCH_TO_PREVIOUS_USER,
  TAKE_PARTIAL_SCREENSHOT,
  TAKE_SCREENSHOT,
  TAKE_WINDOW_SCREENSHOT,
  TOGGLE_APP_LIST,
  TOGGLE_APP_LIST_FULLSCREEN,
  TOGGLE_CAPS_LOCK,
  TOGGLE_DICTATION,
  TOGGLE_DOCKED_MAGNIFIER,
  TOGGLE_FULLSCREEN,
  TOGGLE_FULLSCREEN_MAGNIFIER,
  TOGGLE_HIGH_CONTRAST,
  TOGGLE_MAXIMIZED,
  TOGGLE_MESSAGE_CENTER_BUBBLE,
  TOGGLE_MIRROR_MODE,
  TOGGLE_OVERVIEW,
  TOGGLE_SPOKEN_FEEDBACK,
  TOGGLE_SYSTEM_TRAY_BUBBLE,
  TOGGLE_WIFI,
  TOUCH_HUD_CLEAR,
  TOUCH_HUD_MODE_CHANGE,
  UNPIN,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
  WINDOW_CYCLE_SNAP_LEFT,
  WINDOW_CYCLE_SNAP_RIGHT,
  WINDOW_MINIMIZE,
  MINIMIZE_TOP_WINDOW_ON_BACK,

  // Debug accelerators are intentionally at the end, so that if you remove one
  // you don't need to update tests which check hashes of the ids.
  DEBUG_PRINT_LAYER_HIERARCHY,
  DEBUG_PRINT_VIEW_HIERARCHY,
  DEBUG_PRINT_WINDOW_HIERARCHY,
  DEBUG_SHOW_TOAST,
  DEBUG_TOGGLE_DEVICE_SCALE_FACTOR,
  DEBUG_TOGGLE_SHOW_DEBUG_BORDERS,
  DEBUG_TOGGLE_SHOW_FPS_COUNTER,
  DEBUG_TOGGLE_SHOW_PAINT_RECTS,
  DEBUG_TOGGLE_TOUCH_PAD,
  DEBUG_TOGGLE_TOUCH_SCREEN,
  DEBUG_TOGGLE_TABLET_MODE,
  DEBUG_TOGGLE_WALLPAPER_MODE,
  DEBUG_TRIGGER_CRASH,  // Intentionally crash the ash process.
};

struct AcceleratorData {
  bool trigger_on_press;
  ui::KeyboardCode keycode;
  int modifiers;
  AcceleratorAction action;
};

// A mask of all the modifiers used for debug accelerators.
ASH_PUBLIC_EXPORT constexpr int kDebugModifier =
    ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN;

// Accelerators handled by AcceleratorController.
ASH_PUBLIC_EXPORT extern const AcceleratorData kAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t kAcceleratorDataLength;

// The public-facing interface for accelerator handling, which is Ash's duty to
// implement.
class ASH_PUBLIC_EXPORT AcceleratorController {
 public:
  // Returns the singleton instance.
  static AcceleratorController* Get();

  // Called by Chrome to set the closure that should be run when the volume has
  // been adjusted (playing an audible tone when spoken feedback is enabled).
  static void SetVolumeAdjustmentSoundCallback(
      const base::RepeatingClosure& closure);

  // Called by Ash to run the closure from SetVolumeAdjustmentSoundCallback.
  static void PlayVolumeAdjustmentSound();

  // Activates the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  virtual bool Process(const ui::Accelerator& accelerator) = 0;

  // Returns true if the |accelerator| is deprecated. Deprecated accelerators
  // can be consumed by web contents if needed.
  virtual bool IsDeprecated(const ui::Accelerator& accelerator) const = 0;

  // Performs the specified action if it is enabled. Returns whether the action
  // was performed successfully.
  virtual bool PerformActionIfEnabled(AcceleratorAction action,
                                      const ui::Accelerator& accelerator) = 0;

  // Called by Chrome when a menu item accelerator has been triggered. Returns
  // true if the menu should close.
  virtual bool OnMenuAccelerator(const ui::Accelerator& accelerator) = 0;

  // Returns true if the |accelerator| is registered.
  virtual bool IsRegistered(const ui::Accelerator& accelerator) const = 0;

  // Returns the accelerator histotry.
  virtual ui::AcceleratorHistory* GetAcceleratorHistory() = 0;

 protected:
  AcceleratorController();
  virtual ~AcceleratorController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATORS_H_

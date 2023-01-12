// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATORS_H_
#define ASH_PUBLIC_CPP_ACCELERATORS_H_

#include <stddef.h>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
class Accelerator;
}

namespace ash {
class AcceleratorHistory;

// See documentation in ash/accelerators/accelerator_table.h.

enum AcceleratorAction {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  CYCLE_BACKWARD_MRU,
  CYCLE_FORWARD_MRU,
  CYCLE_SAME_APP_WINDOWS_BACKWARD,
  CYCLE_SAME_APP_WINDOWS_FORWARD,
  DESKS_ACTIVATE_DESK_LEFT,
  DESKS_ACTIVATE_DESK_RIGHT,
  DESKS_MOVE_ACTIVE_ITEM_LEFT,
  DESKS_MOVE_ACTIVE_ITEM_RIGHT,
  DESKS_NEW_DESK,
  DESKS_REMOVE_CURRENT_DESK,
  DESKS_ACTIVATE_0,
  DESKS_ACTIVATE_1,
  DESKS_ACTIVATE_2,
  DESKS_ACTIVATE_3,
  DESKS_ACTIVATE_4,
  DESKS_ACTIVATE_5,
  DESKS_ACTIVATE_6,
  DESKS_ACTIVATE_7,
  DESKS_TOGGLE_ASSIGN_TO_ALL_DESKS,
  DISABLE_CAPS_LOCK,
  EXIT,
  FOCUS_CAMERA_PREVIEW,
  FOCUS_NEXT_PANE,
  FOCUS_PREVIOUS_PANE,
  FOCUS_SHELF,
  FOCUS_PIP,
  KEYBOARD_BACKLIGHT_TOGGLE,
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
  MEDIA_FAST_FORWARD,
  MEDIA_NEXT_TRACK,
  MEDIA_PAUSE,
  MEDIA_PLAY,
  MEDIA_PLAY_PAUSE,
  MEDIA_PREV_TRACK,
  MEDIA_REWIND,
  MEDIA_STOP,
  MICROPHONE_MUTE_TOGGLE,
  MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS,
  NEW_INCOGNITO_WINDOW,
  NEW_TAB,
  NEW_WINDOW,
  OPEN_CALCULATOR,
  OPEN_CROSH,
  OPEN_DIAGNOSTICS,
  OPEN_FEEDBACK_PAGE,
  OPEN_FILE_MANAGER,
  OPEN_GET_HELP,
  // Similar to TOGGLE_CLIPBOARD_HISTORY but is used to paste plain text only
  // when clipboard history menu is already open.
  PASTE_CLIPBOARD_HISTORY_PLAIN_TEXT,
  POWER_PRESSED,
  POWER_RELEASED,
  PRINT_UI_HIERARCHIES,
  PRIVACY_SCREEN_TOGGLE,
  RESTORE_TAB,
  ROTATE_SCREEN,
  ROTATE_WINDOW,
  SCALE_UI_DOWN,
  SCALE_UI_RESET,
  SCALE_UI_UP,
  SHOW_EMOJI_PICKER,
  TOGGLE_IME_MENU_BUBBLE,
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
  TOGGLE_CALENDAR,
  TOGGLE_CAPS_LOCK,
  TOGGLE_CLIPBOARD_HISTORY,
  TOGGLE_DICTATION,
  TOGGLE_DOCKED_MAGNIFIER,
  TOGGLE_FLOATING,
  TOGGLE_FULLSCREEN,
  TOGGLE_FULLSCREEN_MAGNIFIER,
  TOGGLE_HIGH_CONTRAST,
  TOGGLE_MAXIMIZED,
  TOGGLE_MESSAGE_CENTER_BUBBLE,
  TOGGLE_MIRROR_MODE,
  TOGGLE_MULTITASK_MENU,
  TOGGLE_OVERVIEW,
  TOGGLE_PROJECTOR_MARKER,
  TOGGLE_RESIZE_LOCK_MENU,
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
  DEBUG_DUMP_CALENDAR_MODEL,
  DEBUG_KEYBOARD_BACKLIGHT_TOGGLE,
  DEBUG_MICROPHONE_MUTE_TOGGLE,
  DEBUG_PRINT_LAYER_HIERARCHY,
  DEBUG_PRINT_VIEW_HIERARCHY,
  DEBUG_PRINT_WINDOW_HIERARCHY,
  DEBUG_SHOW_TOAST,
  DEBUG_SYSTEM_UI_STYLE_VIEWER,
  // TODO(crbug.com/1336836): Remove fling accelerators after float is released.
  DEBUG_TUCK_FLOATED_WINDOW_LEFT,
  DEBUG_TUCK_FLOATED_WINDOW_RIGHT,
  DEBUG_TOGGLE_DARK_MODE,
  DEBUG_TOGGLE_DYNAMIC_COLOR,
  DEBUG_TOGGLE_GLANCEABLES,
  DEBUG_TOGGLE_SHOW_DEBUG_BORDERS,
  DEBUG_TOGGLE_SHOW_FPS_COUNTER,
  DEBUG_TOGGLE_SHOW_PAINT_RECTS,
  DEBUG_TOGGLE_TOUCH_PAD,
  DEBUG_TOGGLE_TOUCH_SCREEN,
  DEBUG_TOGGLE_TABLET_MODE,
  DEBUG_TOGGLE_VIDEO_CONFERENCE_CAMERA_TRAY_ICON,
  DEBUG_TOGGLE_WALLPAPER_MODE,
  DEBUG_TRIGGER_CRASH,  // Intentionally crash the ash process.
  DEBUG_TOGGLE_HUD_DISPLAY,
  DEV_ADD_REMOVE_DISPLAY,
  // Different than TOGGLE_APP_LIST to ignore search-as-modifier-key rules for
  // enabling the accelerator.
  DEV_TOGGLE_APP_LIST,
  DEV_TOGGLE_UNIFIED_DESKTOP,
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

// Accelerators that are enabled/disabled with new accelerator mapping.
// crbug.com/1067269
ASH_PUBLIC_EXPORT extern const AcceleratorData
    kEnableWithNewMappingAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t
    kEnableWithNewMappingAcceleratorDataLength;
ASH_PUBLIC_EXPORT extern const AcceleratorData
    kDisableWithNewMappingAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t
    kDisableWithNewMappingAcceleratorDataLength;

// Accelerators that are enabled with positional shortcut mapping.
ASH_PUBLIC_EXPORT extern const AcceleratorData
    kEnableWithPositionalAcceleratorsData[];
ASH_PUBLIC_EXPORT extern const size_t
    kEnableWithPositionalAcceleratorsDataLength;

// Accelerators that are enabled with improved desks keyboards shortcuts.
ASH_PUBLIC_EXPORT extern const AcceleratorData
    kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t
    kEnabledWithImprovedDesksKeyboardShortcutsAcceleratorDataLength;

// Accelerators that are enabled with same app window cycling experiment.
ASH_PUBLIC_EXPORT extern const AcceleratorData
    kEnableWithSameAppWindowCycleAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t
    kEnableWithSameAppWindowCycleAcceleratorDataLength;

// Accelerators that are enabled with the floating windows feature.
ASH_PUBLIC_EXPORT extern const AcceleratorData
    kEnableWithFloatWindowAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t
    kEnableWithFloatWindowAcceleratorDataLength;

// The public-facing interface for accelerator handling, which is Ash's duty to
// implement.
class ASH_PUBLIC_EXPORT AcceleratorController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when `action` is performed.
    virtual void OnActionPerformed(AcceleratorAction action) = 0;
    // Invoked when `controller` is destroyed.
    virtual void OnAcceleratorControllerWillBeDestroyed(
        AcceleratorController* controller) {}
  };

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
  virtual AcceleratorHistory* GetAcceleratorHistory() = 0;

  // Returns true if the provided accelerator matches the provided accelerator
  // action.
  virtual bool DoesAcceleratorMatchAction(const ui::Accelerator& accelerator,
                                          const AcceleratorAction action) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  AcceleratorController();
  virtual ~AcceleratorController();
  void NotifyActionPerformed(AcceleratorAction action);

  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

// The public facing interface for AcceleratorHistory, which is implemented in
// ash.
class ASH_PUBLIC_EXPORT AcceleratorHistory {
 public:
  // Stores |accelerator| if it's different than the currently stored one.
  virtual void StoreCurrentAccelerator(const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCELERATORS_H_

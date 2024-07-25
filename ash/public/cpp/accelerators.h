// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCELERATORS_H_
#define ASH_PUBLIC_CPP_ACCELERATORS_H_

#include <stddef.h>

#include "ash/public/cpp/accelerator_actions.h"
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

struct AcceleratorData {
  bool trigger_on_press;
  ui::KeyboardCode keycode;
  int modifiers;
  AcceleratorAction action;
  bool accelerator_locked = false;
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

// Accelerators that are enabled with the game dashboard feature.
ASH_PUBLIC_EXPORT extern const AcceleratorData
    kToggleGameDashboardAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t kToggleGameDashboardAcceleratorDataLength;

// Accelerators that are enabled with the Picker feature.
ASH_PUBLIC_EXPORT extern const AcceleratorData kTogglePickerAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t kTogglePickerAcceleratorDataLength;

ASH_PUBLIC_EXPORT extern const AcceleratorData
    kTilingWindowResizeAcceleratorData[];
ASH_PUBLIC_EXPORT extern const size_t kTilingWindowResizeAcceleratorDataLength;

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

  // Returns true if |key_code| is a key usually handled directly by the shell.
  static bool IsSystemKey(ui::KeyboardCode key_code);

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

  virtual void ApplyAcceleratorForTesting(
      const ui::Accelerator& accelerator) = 0;

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

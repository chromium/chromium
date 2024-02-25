// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_NOTE_DISPLAY_STATE_HANDLER_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_NOTE_DISPLAY_STATE_HANDLER_H_

#include <memory>

#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"

namespace ash {

class LockScreenNoteLauncher;
class ScopedBacklightsForcedOff;

// Handles display state changes related to lock screen note state.
// For example it will close any active lock screen notes if the display is
// forced off.
// This class also handles a lock screen note launch when stylus is ejected.
// When the note is launched while the screen is off, note launch forces the
// display off, in order to delay screen being turned on (which happens, among
// other things, when the stylus gets ejected) until the lock screen note is
// visible. This is to prevent a flash of the lock screen UI as the lock screen
// note app window is being shown.
class LockScreenNoteDisplayStateHandler : public ScreenBacklightObserver {
 public:
  explicit LockScreenNoteDisplayStateHandler(
      BacklightsForcedOffSetter* backlights_forced_off_setter);

  LockScreenNoteDisplayStateHandler(const LockScreenNoteDisplayStateHandler&) =
      delete;
  LockScreenNoteDisplayStateHandler& operator=(
      const LockScreenNoteDisplayStateHandler&) = delete;

  ~LockScreenNoteDisplayStateHandler() override;

  base::OneShotTimer* launch_timer_for_test() { return &launch_timer_; }

  // ScreenBacklightObserver:
  void OnBacklightsForcedOffChanged(bool backlights_forced_off) override;
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

  // If lock screen note action is available, it requests a new lock screen note
  // with launch reason set to stylus eject.
  void AttemptNoteLaunchForStylusEject();

  // Resets the internal state, cancelling any in progress launch.
  void Reset();

 private:
  // Runs lock screen note launcher, which starts lock screen app launch.
  void RunLockScreenNoteLauncher();

  // Whether the backlights should be forced off during lock screen note
  // launch.
  bool ShouldForceBacklightsOffForNoteLaunch() const;

  // Whether a lock screen note is currently being launched by |this|.
  bool NoteLaunchInProgressOrDelayed() const;

  // Called by |lock_screen_note_launcher_| when lock screen note launch is
  // done.
  void NoteLaunchDone(bool success);

  // Object used to force the backlights off.
  const raw_ptr<BacklightsForcedOffSetter> backlights_forced_off_setter_;

  // Whether lock screen note launch is delayed until the screen is reported to
  // be off - this is used if lock screen note launch is requested when
  // backlights have been forced off, but the power manager still reports screen
  // to be on.
  bool note_launch_delayed_until_screen_off_ = false;

  std::unique_ptr<LockScreenNoteLauncher> lock_screen_note_launcher_;

  // Scoped backlights forced off request - this is returned by
  // |backlights_forced_off_setter_->ForceBacklightsOff()|, and will keep the
  // backlights in forced-off state until they are reset.
  std::unique_ptr<ScopedBacklightsForcedOff> backlights_forced_off_;

  // Timer used to set up timeout for lock screen note launch.
  base::OneShotTimer launch_timer_;

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      backlights_forced_off_observation_;

  base::WeakPtrFactory<LockScreenNoteDisplayStateHandler> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_NOTE_DISPLAY_STATE_HANDLER_H_

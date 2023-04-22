// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_note_display_state_handler.h"

#include <utility>

#include "ash/lock_screen_action/lock_screen_note_launcher.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "ash/shell.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/tray_action/tray_action.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"

namespace ash {

namespace {

// The max amount of time display state change handling can be delayed due to a
// lock screen note action launch. The time starts running when the app launch
// is requested.
constexpr base::TimeDelta kNoteLaunchTimeout = base::Milliseconds(1500);

}  // namespace

LockScreenNoteDisplayStateHandler::LockScreenNoteDisplayStateHandler(
    BacklightsForcedOffSetter* backlights_forced_off_setter)
    : backlights_forced_off_setter_(backlights_forced_off_setter),
      backlights_forced_off_observation_(this) {
  backlights_forced_off_observation_.Observe(
      backlights_forced_off_setter_.get());
}

LockScreenNoteDisplayStateHandler::~LockScreenNoteDisplayStateHandler() =
    default;

void LockScreenNoteDisplayStateHandler::OnBacklightsForcedOffChanged(
    bool backlights_forced_off) {
  // Close lock screen note when backlights are forced off - unless the
  // backlights are forced off by this as part of note app launch.
  if (backlights_forced_off && !backlights_forced_off_) {
    Shell::Get()->tray_action()->CloseLockScreenNote(
        mojom::CloseLockScreenNoteReason::kScreenDimmed);
  }
}

void LockScreenNoteDisplayStateHandler::OnScreenBacklightStateChanged(
    ScreenBacklightState screen_backlight_state) {
  if (screen_backlight_state != ScreenBacklightState::ON &&
      note_launch_delayed_until_screen_off_) {
    RunLockScreenNoteLauncher();
  }
}

void LockScreenNoteDisplayStateHandler::AttemptNoteLaunchForStylusEject() {
  if (!LockScreenNoteLauncher::CanAttemptLaunch() ||
      NoteLaunchInProgressOrDelayed()) {
    return;
  }

  if (!backlights_forced_off_ && ShouldForceBacklightsOffForNoteLaunch()) {
    backlights_forced_off_ =
        backlights_forced_off_setter_->ForceBacklightsOff();
  }

  DCHECK(!launch_timer_.IsRunning());
  launch_timer_.Start(
      FROM_HERE, kNoteLaunchTimeout,
      base::BindOnce(&LockScreenNoteDisplayStateHandler::NoteLaunchDone,
                     weak_ptr_factory_.GetWeakPtr(), false));

  // Delay note launch if backlights are forced off, but the screen hasn't
  // been turned off yet - the note should be launched when the pending
  // backlights state is finished (i.e. the screen is turned off).
  if (backlights_forced_off_setter_->backlights_forced_off() &&
      backlights_forced_off_setter_->GetScreenBacklightState() ==
          ScreenBacklightState::ON) {
    note_launch_delayed_until_screen_off_ = true;
    return;
  }

  RunLockScreenNoteLauncher();
}

void LockScreenNoteDisplayStateHandler::Reset() {
  note_launch_delayed_until_screen_off_ = false;
  backlights_forced_off_.reset();
  lock_screen_note_launcher_.reset();
  launch_timer_.Stop();
}

void LockScreenNoteDisplayStateHandler::RunLockScreenNoteLauncher() {
  DCHECK(!lock_screen_note_launcher_);
  if (!LockScreenNoteLauncher::CanAttemptLaunch()) {
    Reset();
    return;
  }

  note_launch_delayed_until_screen_off_ = false;

  lock_screen_note_launcher_ = std::make_unique<LockScreenNoteLauncher>();
  lock_screen_note_launcher_->Run(
      mojom::LockScreenNoteOrigin::kStylusEject,
      base::BindOnce(&LockScreenNoteDisplayStateHandler::NoteLaunchDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool LockScreenNoteDisplayStateHandler::ShouldForceBacklightsOffForNoteLaunch()
    const {
  // Backlights should be kept off during app launch if the display has been
  // turned off without user interaction (e.g. due to user inactivity), or if
  // the backlights are currently being forced off - the goal is to avoid flash
  // of lock screen UI if the display gets turned on before lock screen app
  // window is shown.
  // There is no need to force the backlight off if the display has been turned
  // off due to user action - in this case display brightness will not change
  // when backlights stop being forced off (due to stylus eject) - the
  // brightness will remain at user selected level, so the lock screen UI will
  // not actually become visible.
  //
  // Note that backlights_forced_off_setter_ check is required as there is a
  // delay between request to force backlights off and screen state getting
  // updated due to that request.
  return backlights_forced_off_setter_->backlights_forced_off() ||
         backlights_forced_off_setter_->GetScreenBacklightState() ==
             ScreenBacklightState::OFF_AUTO;
}

bool LockScreenNoteDisplayStateHandler::NoteLaunchInProgressOrDelayed() const {
  return note_launch_delayed_until_screen_off_ || lock_screen_note_launcher_;
}

void LockScreenNoteDisplayStateHandler::NoteLaunchDone(bool success) {
  Reset();
}

}  // namespace ash

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_NOTE_LAUNCHER_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_NOTE_LAUNCHER_H_

#include "ash/ash_export.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"

namespace ash {

// A helper class for requesting a lock screen app that provides a callback run
// when the action launch process finishes (both successfuly or with a failure).
class ASH_EXPORT LockScreenNoteLauncher : public TrayActionObserver {
 public:
  using LaunchCallback = base::OnceCallback<void(bool success)>;

  LockScreenNoteLauncher();

  LockScreenNoteLauncher(const LockScreenNoteLauncher&) = delete;
  LockScreenNoteLauncher& operator=(const LockScreenNoteLauncher&) = delete;

  ~LockScreenNoteLauncher() override;

  // Whether the lock screen note state indicates that a note action launch can
  // be requested - note that |Run| will not succeed if this returns false.
  static bool CanAttemptLaunch();

  // Requests a lock screen note launch, and starts observing lock screen note
  // state changes - when the state changes to a non-launching state (either
  // kActive - indicating launch success, or kAvailable or kNotAvailable -
  // indicating launch failure), it runs |callback|.
  // This can handle only one launch requests at a time - i.e. it should not be
  // called again before |callback| is run.
  // Returns whether the note launch was successfully requested. |callback| will
  // not be called if the return value is false.
  bool Run(mojom::LockScreenNoteOrigin action_origin, LaunchCallback callback);

  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

 private:
  // Called when the launch attempt completes - resets the object state and runs
  // the launch callback provided to |Run|.
  void OnLaunchDone(bool success);

  // The callback provided to |Run|.
  LaunchCallback callback_;

  base::ScopedObservation<TrayAction, TrayActionObserver>
      tray_action_observation_{this};
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_NOTE_LAUNCHER_H_

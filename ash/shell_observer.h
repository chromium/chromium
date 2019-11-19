// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_OBSERVER_H_
#define ASH_SHELL_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/login_status.h"

namespace aura {
class Window;
}

namespace ash {

class ASH_EXPORT ShellObserver {
 public:
  // Called when a casting session is started or stopped.
  virtual void OnCastingSessionStartedOrStopped(bool started) {}

  // Invoked after a non-primary root window is created.
  virtual void OnRootWindowAdded(aura::Window* root_window) {}

  // Invoked when the shelf alignment in |root_window| is changed.
  virtual void OnShelfAlignmentChanged(aura::Window* root_window) {}

  // Invoked when user work area insets (accessibility panel, docked magnifier,
  // keyboard) in |root_window| changed.
  // This notification is not fired when shelf bounds changed.
  virtual void OnUserWorkAreaInsetsChanged(aura::Window* root_window) {}

  // Invoked when the shelf auto-hide behavior in |root_window| is changed.
  virtual void OnShelfAutoHideBehaviorChanged(aura::Window* root_window) {}

  // Invoked when entering or exiting fullscreen mode in |container|.
  // |container| is always the active desk container.
  virtual void OnFullscreenStateChanged(bool is_fullscreen,
                                        aura::Window* container) {}

  // Invoked when |pinned_window| enter or exit pinned mode.
  virtual void OnPinnedStateChanged(aura::Window* pinned_window) {}

  // Called when dictation is activated.
  virtual void OnDictationStarted() {}

  // Called when dicatation is ended.
  virtual void OnDictationEnded() {}

  // Called at the end of Shell::Init.
  virtual void OnShellInitialized() {}

  // Called at the beginning of ~Shell.
  virtual void OnShellDestroying() {}

  // Called near the end of ~Shell. Shell::Get() still returns the Shell, but
  // most of Shell's state has been deleted.
  virtual void OnShellDestroyed() {}

 protected:
  virtual ~ShellObserver() {}
};

}  // namespace ash

#endif  // ASH_SHELL_OBSERVER_H_

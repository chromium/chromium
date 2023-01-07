// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_FOCUS_CYCLER_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_FOCUS_CYCLER_DELEGATE_H_

#include "base/functional/callback_forward.h"

namespace lock_screen_apps {

// Used by the StateController to inject a lock screen app window in the tab
// order cycle with the lock screen UI and system tray, i.e. to move the focus
// away from the app window when the app window is tabbed through, and to
// register a handler for receiving focus from the lock screen UI.
class FocusCyclerDelegate {
 public:
  virtual ~FocusCyclerDelegate() = default;

  // Registers a callback that should be called when the focus should be moved
  // to the app window.
  using LockScreenAppFocusCallback =
      base::RepeatingCallback<void(bool reverse)>;
  virtual void RegisterLockScreenAppFocusHandler(
      const LockScreenAppFocusCallback& focus_handler) = 0;

  // Unregister the callback that should be called to move the focus to the
  // app window, if one was registered.
  virtual void UnregisterLockScreenAppFocusHandler() = 0;

  // Called when the focus leaves the lock screen app window. The delegate
  // should move the focus to the next appropriate UI element.
  virtual void HandleLockScreenAppFocusOut(bool reverse) = 0;
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_ASH_LOCK_SCREEN_APPS_FOCUS_CYCLER_DELEGATE_H_

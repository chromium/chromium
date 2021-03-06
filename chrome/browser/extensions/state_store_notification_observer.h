// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_STATE_STORE_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_STATE_STORE_NOTIFICATION_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/sessions/session_restore.h"

namespace extensions {
class StateStore;

// Initializes the StateStore when session restore is complete, for example when
// page load notifications are not sent ("Continue where I left off").
// http://crbug.com/230481
class StateStoreNotificationObserver {
 public:
  explicit StateStoreNotificationObserver(StateStore* state_store);
  ~StateStoreNotificationObserver();

 private:
  // Called when a session restore has finished.
  void OnSessionRestoreDone(int num_tabs_restored);

  StateStore* state_store_;  // Not owned.

  // Points to the on-session-restored callback that was registered with
  // SessionRestore's callback list. When objects of this class are destroyed,
  // the subscription's destructor will automatically unregister the callback in
  // SessionRestore, so that the callback list does not contain any obsolete
  // callbacks.
  base::CallbackListSubscription on_session_restored_callback_subscription_;

  DISALLOW_COPY_AND_ASSIGN(StateStoreNotificationObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_STATE_STORE_NOTIFICATION_OBSERVER_H_

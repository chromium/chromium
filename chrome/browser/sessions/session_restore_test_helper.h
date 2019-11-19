// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sessions/session_restore.h"

namespace content {
class MessageLoopRunner;
}

// This class waits on the SessionRestore notification. It is a replacement for
// content::WindowedNotificationObserver, which uses notification service. This
// class uses the callback-based notification instead.
class SessionRestoreTestHelper {
 public:
  SessionRestoreTestHelper();
  ~SessionRestoreTestHelper();

  // Blocks until OnSessionRestore() is called.
  void Wait();

 private:
  // Callback for session restore notifications.
  void OnSessionRestoreDone(int /* num_tabs_restored */);

  // Indicates whether a session restore notification has been received.
  bool restore_notification_seen_;

  // Indicates whether |message_loop_runner_| is running.
  bool loop_is_running_;

  // Loop that runs while waiting for notification callback.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  // For automatically unsubscribing from callback-based notifications.
  SessionRestore::CallbackSubscription callback_subscription_;

  // For safely binding pointers to callbacks.
  base::WeakPtrFactory<SessionRestoreTestHelper> weak_ptr_factory{this};

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreTestHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_HELPER_H_

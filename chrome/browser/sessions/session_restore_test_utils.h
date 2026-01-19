// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_UTILS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_UTILS_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"

class Profile;

namespace testing {

// This class waits for a specified number of sessions to be restored.
class SessionsRestoredWaiter {
 public:
  explicit SessionsRestoredWaiter(base::OnceClosure quit_closure,
                                  int num_session_restores_expected);
  SessionsRestoredWaiter(const SessionsRestoredWaiter&) = delete;
  SessionsRestoredWaiter& operator=(const SessionsRestoredWaiter&) = delete;
  ~SessionsRestoredWaiter();

 private:
  // Callback for session restore notifications.
  void OnSessionRestoreDone(Profile* profile, int num_tabs_restored);

  // For automatically unsubscribing from callback-based notifications.
  base::CallbackListSubscription callback_subscription_;
  base::OnceClosure quit_closure_;
  int num_session_restores_expected_;
  int num_sessions_restored_ = 0;
};

}  // namespace testing

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_TEST_UTILS_H_

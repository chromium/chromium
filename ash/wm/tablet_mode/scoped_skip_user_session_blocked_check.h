// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_SCOPED_SKIP_USER_SESSION_BLOCKED_CHECK_H_
#define ASH_WM_TABLET_MODE_SCOPED_SKIP_USER_SESSION_BLOCKED_CHECK_H_

namespace ash {

// ScopedSkipUserSessionBlockedCheck allows for skipping checks for if the user
// session is blocked in the event client for a short region of code within a
// scope.
class ScopedSkipUserSessionBlockedCheck {
 public:
  ScopedSkipUserSessionBlockedCheck();

  ScopedSkipUserSessionBlockedCheck(const ScopedSkipUserSessionBlockedCheck&) =
      delete;
  ScopedSkipUserSessionBlockedCheck& operator=(
      const ScopedSkipUserSessionBlockedCheck&) = delete;

  ~ScopedSkipUserSessionBlockedCheck();
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_SCOPED_SKIP_USER_SESSION_BLOCKED_CHECK_H_

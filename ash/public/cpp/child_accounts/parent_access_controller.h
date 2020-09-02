// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_H_
#define ASH_PUBLIC_CPP_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_H_

namespace ash {

// Actions that might require parental approval.
enum class SupervisedAction {
  // Unlock a Chromebook that is locked due to a Time Limit policy.
  kUnlockTimeLimits,
  // When Chrome is unable to automatically verify if the OS time is correct
  // the user becomes able to manually change the clock. The entry points are
  // the settings page (in-session) and the tray bubble (out-session).
  kUpdateClock,
  // Change timezone from the settings page.
  kUpdateTimezone,
  // Add user flow.
  kAddUser,
  // Re-authentication flow.
  kReauth,
};

// TODO(crbug.com/1123722): Define ParentAccessController interface.

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CHILD_ACCOUNTS_PARENT_ACCESS_CONTROLLER_H_
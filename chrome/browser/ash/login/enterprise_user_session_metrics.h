// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENTERPRISE_USER_SESSION_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_ENTERPRISE_USER_SESSION_METRICS_H_

#include "base/time/time.h"
#include "components/user_manager/user_type.h"

class PrefRegistrySimple;

namespace ash {

class UserContext;

namespace enterprise_user_session_metrics {

// Enum for logins metrics on an enrolled device.
enum class SignInEventType {
  // A regular user login.
  REGULAR_USER = 0,
  // Manually started public session.
  MANUAL_PUBLIC_SESSION = 1,
  // Automatically started public session.
  AUTOMATIC_PUBLIC_SESSION = 2,
  // Manually started kiosk session.
  MANUAL_KIOSK = 3,
  // Automatically started kiosk session.
  AUTOMATIC_KIOSK = 4,
  // Count of sign-in event types. Must be the last one.
  SIGN_IN_EVENT_COUNT,
};

// Register local state preferences.
void RegisterPrefs(PrefRegistrySimple* registry);

// Records a sign-in event for an enrolled device.
void RecordSignInEvent(SignInEventType sign_in_event_type);

// Records a sign-in event by UserContext for an enrolled device.
// `is_auto_login` indicates whether the sign-in is a policy configured
// automatic login or a manual login in response to user action.
void RecordSignInEvent(const UserContext& user_context, bool is_auto_login);

// Stores session length for regular user, public session user for enrolled
// device to be reported on the next run. It stores the duration in a local
// state pref instead of sending it to metrics code directly because it is
// called on shutdown path and metrics are likely to be lost. The stored value
// would be reported on the next run.
void StoreSessionLength(user_manager::UserType session_type,
                        const base::TimeDelta& session_length);

// Records the stored session length and clears it.
void RecordStoredSessionLength();

}  // namespace enterprise_user_session_metrics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENTERPRISE_USER_SESSION_METRICS_H_

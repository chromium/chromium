// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enterprise_user_session_metrics.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

// Returns true if the device is enterprise managed, false otherwise.
bool IsEnterpriseManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->IsEnterpriseManaged();
}

// Returns the duration in minutes, capped at |max_duration| and rounded down to
// the nearest |bucket_size| minutes.
int GetMinutesToReport(base::TimeDelta duration,
                       int bucket_size,
                       base::TimeDelta max_duration) {
  int minutes = std::min(duration.InMinutes(), max_duration.InMinutes());
  return minutes / bucket_size * bucket_size;
}

}  // namespace

namespace chromeos {
namespace enterprise_user_session_metrics {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kLastSessionType, 0);
  registry->RegisterInt64Pref(prefs::kLastSessionLength, 0);
}

void RecordSignInEvent(SignInEventType sign_in_event_type) {
  DCHECK(IsEnterpriseManaged());

  UMA_HISTOGRAM_ENUMERATION(
      "Enterprise.UserSession.Logins", static_cast<int>(sign_in_event_type),
      static_cast<int>(SignInEventType::SIGN_IN_EVENT_COUNT));
}

void RecordSignInEvent(const UserContext& user_context, bool is_auto_login) {
  DCHECK(IsEnterpriseManaged());

  const user_manager::UserType session_type = user_context.GetUserType();
  if (session_type == user_manager::USER_TYPE_REGULAR) {
    RecordSignInEvent(SignInEventType::REGULAR_USER);
  } else if (session_type == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    RecordSignInEvent(is_auto_login ? SignInEventType::AUTOMATIC_PUBLIC_SESSION
                                    : SignInEventType::MANUAL_PUBLIC_SESSION);
  }

  // Kiosk sign-ins are handled separately in AppLaunchController and other
  // session types are ignored for now.
}

void StoreSessionLength(user_manager::UserType session_type,
                        const base::TimeDelta& session_length) {
  DCHECK(IsEnterpriseManaged());

  if (session_type != user_manager::USER_TYPE_REGULAR &&
      session_type != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    // No session length metric for other session types.
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kLastSessionType, session_type);
  local_state->SetInt64(prefs::kLastSessionLength,
                        session_length.ToInternalValue());
  local_state->CommitPendingWrite();
}

void RecordStoredSessionLength() {
  DCHECK(IsEnterpriseManaged());

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state->HasPrefPath(prefs::kLastSessionType) ||
      !local_state->HasPrefPath(prefs::kLastSessionLength)) {
    return;
  }

  const user_manager::UserType session_type =
      static_cast<user_manager::UserType>(
          local_state->GetInteger(prefs::kLastSessionType));
  const base::TimeDelta session_length = base::TimeDelta::FromInternalValue(
      local_state->GetInt64(prefs::kLastSessionLength));

  local_state->ClearPref(prefs::kLastSessionType);
  local_state->ClearPref(prefs::kLastSessionLength);

  if (session_length.is_zero())
    return;

  std::string metric_name;
  if (session_type == user_manager::USER_TYPE_REGULAR) {
    metric_name = "Enterprise.RegularUserSession.SessionLength";
  } else if (session_type == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    metric_name = "Enterprise.PublicSession.SessionLength";
  } else {
    // NOTREACHED() since session length for other session types should not
    // be recorded.
    NOTREACHED();
    return;
  }

  // Report session duration for the first 24 hours, split into 144 buckets
  // (i.e. every 10 minute). Note that sparse histogram is used here. It is
  // important to limit the number of buckets to something reasonable.
  base::UmaHistogramSparse(
      metric_name,
      GetMinutesToReport(session_length, 10, base::TimeDelta::FromHours(24)));

  if (chromeos::DemoSession::IsDeviceInDemoMode()) {
    // Demo mode sessions will have shorter durations. Report session length
    // rounded down to the nearest minute, up to two hours.
    base::UmaHistogramSparse(
        "DemoMode.SessionLength",
        GetMinutesToReport(session_length, 1, base::TimeDelta::FromHours(2)));
  }
}

}  // namespace enterprise_user_session_metrics
}  // namespace chromeos

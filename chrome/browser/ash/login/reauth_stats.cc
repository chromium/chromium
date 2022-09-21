// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reauth_stats.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {

ReauthReason GetReauthReason(const user_manager::KnownUser& known_user,
                             const AccountId& account_id) {
  return static_cast<ReauthReason>(
      known_user.FindReauthReason(account_id)
          .value_or(static_cast<int>(ReauthReason::NONE)));
}

}  // namespace

void RecordReauthReason(const AccountId& account_id, ReauthReason reason) {
  if (reason == ReauthReason::NONE)
    return;
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (GetReauthReason(known_user, account_id) == reason)
    return;

  LOG(WARNING) << "Reauth reason updated: " << static_cast<int>(reason);
  known_user.UpdateReauthReason(account_id, static_cast<int>(reason));
}

void SendReauthReason(const AccountId& account_id, bool password_changed) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  ReauthReason reauth_reason = GetReauthReason(known_user, account_id);
  if (reauth_reason == ReauthReason::NONE)
    return;
  if (password_changed) {
    base::UmaHistogramEnumeration("Login.PasswordChanged.ReauthReason",
                                  reauth_reason,
                                  ReauthReason::NUM_REAUTH_FLOW_REASONS);
  } else {
    base::UmaHistogramEnumeration("Login.PasswordNotChanged.ReauthReason",
                                  reauth_reason,
                                  ReauthReason::NUM_REAUTH_FLOW_REASONS);
  }
  known_user.UpdateReauthReason(account_id,
                                static_cast<int>(ReauthReason::NONE));
}

}  // namespace ash

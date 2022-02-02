// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reauth_stats.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/known_user.h"

namespace ash {

void RecordReauthReason(const AccountId& account_id, ReauthReason reason) {
  if (reason == ReauthReason::NONE)
    return;
  user_manager::KnownUser known_user(g_browser_process->local_state());
  // We record only the first value, skipping everything else, except "none"
  // value, which is used to reset the current state.
  if (known_user.FindReauthReason(account_id).value_or(ReauthReason::NONE) ==
      reason) {
    return;
  }
  LOG(WARNING) << "Reauth reason updated: " << reason;
  known_user.UpdateReauthReason(account_id, static_cast<int>(reason));
}

void SendReauthReason(const AccountId& account_id, bool password_changed) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  ReauthReason reauth_reason = static_cast<ReauthReason>(
      known_user.FindReauthReason(account_id).value_or(ReauthReason::NONE));
  if (reauth_reason == ReauthReason::NONE)
    return;
  if (password_changed) {
    base::UmaHistogramEnumeration("Login.PasswordChanged.ReauthReason",
                                  reauth_reason, NUM_REAUTH_FLOW_REASONS);
  } else {
    base::UmaHistogramEnumeration("Login.PasswordNotChanged.ReauthReason",
                                  reauth_reason, NUM_REAUTH_FLOW_REASONS);
  }
  known_user.UpdateReauthReason(account_id,
                                static_cast<int>(ReauthReason::NONE));
}

}  // namespace ash

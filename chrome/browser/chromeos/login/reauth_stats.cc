// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/reauth_stats.h"

#include "base/metrics/histogram_macros.h"
#include "components/user_manager/known_user.h"

namespace chromeos {

void RecordReauthReason(const AccountId& account_id, ReauthReason reason) {
  int old_reason;
  // We record only the first value, skipping everything else, except "none"
  // value, which is used to reset the current state.
  if (!user_manager::known_user::FindReauthReason(account_id, &old_reason) ||
      (static_cast<ReauthReason>(old_reason) == ReauthReason::NONE &&
       reason != ReauthReason::NONE)) {
    VLOG(1) << "Reauth reason updated: " << reason;
    user_manager::known_user::UpdateReauthReason(account_id,
                                                 static_cast<int>(reason));
  }
}

void SendReauthReason(const AccountId& account_id) {
  int reauth_reason_int;
  if (!user_manager::known_user::FindReauthReason(account_id,
                                                  &reauth_reason_int)) {
    return;
  }
  ReauthReason reauth_reason = static_cast<ReauthReason>(reauth_reason_int);
  if (reauth_reason != ReauthReason::NONE) {
    UMA_HISTOGRAM_ENUMERATION("Login.ReauthReason", reauth_reason,
                              NUM_REAUTH_FLOW_REASONS);
    user_manager::known_user::UpdateReauthReason(
        account_id, static_cast<int>(ReauthReason::NONE));
  }
}

}  // namespace chromeos

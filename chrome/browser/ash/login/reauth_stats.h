// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REAUTH_STATS_H_
#define CHROME_BROWSER_ASH_LOGIN_REAUTH_STATS_H_

#include "ash/public/cpp/reauth_reason.h"

class AccountId;

namespace ash {

void RecordReauthReason(const AccountId& account_id, ReauthReason reason);
void SendReauthReason(const AccountId& account_id, bool password_changed);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_REAUTH_STATS_H_

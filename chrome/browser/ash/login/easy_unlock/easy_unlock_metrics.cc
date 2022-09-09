// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

void RecordEasyUnlockDidUserManuallyUnlockPhone(bool did_unlock) {
  UMA_HISTOGRAM_BOOLEAN("EasyUnlock.AuthEvent.DidUserManuallyUnlockPhone",
                        did_unlock);
}

void RecordEasyUnlockSigninDuration(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("EasyUnlock.AuthEvent.SignIn.Duration", duration);
}

void RecordEasyUnlockSigninEvent(EasyUnlockAuthEvent event) {
  DCHECK_LT(event, EASY_UNLOCK_AUTH_EVENT_COUNT);
  UMA_HISTOGRAM_ENUMERATION("EasyUnlock.AuthEvent.SignIn", event,
                            EASY_UNLOCK_AUTH_EVENT_COUNT);
}

void RecordEasyUnlockScreenUnlockDuration(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("EasyUnlock.AuthEvent.Unlock.Duration", duration);
}

void RecordEasyUnlockScreenUnlockEvent(EasyUnlockAuthEvent event) {
  DCHECK_LT(event, EASY_UNLOCK_AUTH_EVENT_COUNT);
  UMA_HISTOGRAM_ENUMERATION("EasyUnlock.AuthEvent.Unlock", event,
                            EASY_UNLOCK_AUTH_EVENT_COUNT);
}

}  // namespace ash

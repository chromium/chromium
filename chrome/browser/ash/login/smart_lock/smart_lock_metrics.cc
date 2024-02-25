// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/smart_lock_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

void RecordSmartLockDidUserManuallyUnlockPhone(bool did_unlock) {
  UMA_HISTOGRAM_BOOLEAN("EasyUnlock.AuthEvent.DidUserManuallyUnlockPhone",
                        did_unlock);
}

void RecordSmartLockSigninDuration(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("EasyUnlock.AuthEvent.SignIn.Duration", duration);
}

void RecordSmartLockSigninEvent(SmartLockAuthEvent event) {
  DCHECK_LT(event, SMART_LOCK_AUTH_EVENT_COUNT);
  UMA_HISTOGRAM_ENUMERATION("EasyUnlock.AuthEvent.SignIn", event,
                            SMART_LOCK_AUTH_EVENT_COUNT);
}

void RecordSmartLockScreenUnlockDuration(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("EasyUnlock.AuthEvent.Unlock.Duration", duration);
}

void RecordSmartLockScreenUnlockEvent(SmartLockAuthEvent event) {
  DCHECK_LT(event, SMART_LOCK_AUTH_EVENT_COUNT);
  UMA_HISTOGRAM_ENUMERATION("EasyUnlock.AuthEvent.Unlock", event,
                            SMART_LOCK_AUTH_EVENT_COUNT);
}

}  // namespace ash

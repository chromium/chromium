// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/lock_or_suspend_detector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "ui/base/idle/idle.h"

namespace ttc {

LockOrSuspendDetector::LockOrSuspendDetector(
    base::RepeatingCallback<void(Reason)> callback)
    : callback_(std::move(callback)) {
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  // `base::Unretained()` is safe because the subscription is owned by this
  // object.
  screen_lock_subscription_ = ui::AddScreenLockCallback(base::BindRepeating(
      &LockOrSuspendDetector::OnScreenLockChanged, base::Unretained(this)));
}

LockOrSuspendDetector::~LockOrSuspendDetector() {
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

void LockOrSuspendDetector::OnSuspend() {
  callback_.Run(Reason::kSuspend);
}

void LockOrSuspendDetector::OnScreenLockChanged(bool locked) {
  if (locked) {
    callback_.Run(Reason::kScreenLock);
  }
}

}  // namespace ttc

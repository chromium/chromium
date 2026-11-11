// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_LOCK_OR_SUSPEND_DETECTOR_H_
#define CHROME_BROWSER_TTC_CORE_LOCK_OR_SUSPEND_DETECTOR_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/power_monitor/power_observer.h"

namespace ttc {

// Invokes a callback when the device is about to suspend or the screen locks,
// so a session doesn't keep the microphone open while the user is away.
class LockOrSuspendDetector : public base::PowerSuspendObserver {
 public:
  enum class Reason {
    kSuspend,
    kScreenLock,
  };

  explicit LockOrSuspendDetector(
      base::RepeatingCallback<void(Reason)> callback);
  LockOrSuspendDetector(const LockOrSuspendDetector&) = delete;
  LockOrSuspendDetector& operator=(const LockOrSuspendDetector&) = delete;
  ~LockOrSuspendDetector() override;

  // base::PowerSuspendObserver:
  void OnSuspend() override;

  // Called by the screen lock subscription. Visible for testing:
  // `ui::AddScreenLockCallback()` has no fake.
  void OnScreenLockChanged(bool locked);

 private:
  base::RepeatingCallback<void(Reason)> callback_;
  base::CallbackListSubscription screen_lock_subscription_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_LOCK_OR_SUSPEND_DETECTOR_H_

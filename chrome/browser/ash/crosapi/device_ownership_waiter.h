// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DEVICE_OWNERSHIP_WAITER_H_
#define CHROME_BROWSER_ASH_CROSAPI_DEVICE_OWNERSHIP_WAITER_H_

#include "base/functional/callback_forward.h"

namespace crosapi {

class DeviceOwnershipWaiter {
 public:
  DeviceOwnershipWaiter();
  DeviceOwnershipWaiter(const DeviceOwnershipWaiter&) = delete;
  DeviceOwnershipWaiter& operator=(const DeviceOwnershipWaiter&) = delete;
  virtual ~DeviceOwnershipWaiter();

  // Delays execution of `callback` until the device owner is initialized in
  // `UserManager`. The delay is skipped (and the callback invoked immediately)
  // in the following cases:
  // - we are launching at the login screen: The device owner might not be
  // determined yet.
  // - this is a guest session: Guest sessions can occur before the initial OOBE
  // and are by design without an owner.
  // - this is a demo mode session: Same as guest session.
  // - we are running ChromeOS on Linux: The `DeviceSettingsService` is not
  // behaving as in the real world for these builds, hence we can skip the
  // check.
  virtual void WaitForOwnershipFetched(base::OnceClosure callback,
                                       bool launching_at_login_screen) = 0;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DEVICE_OWNERSHIP_WAITER_H_

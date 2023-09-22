// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FAKE_DEVICE_OWNERSHIP_WAITER_H_
#define CHROME_BROWSER_ASH_CROSAPI_FAKE_DEVICE_OWNERSHIP_WAITER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/crosapi/device_ownership_waiter.h"

namespace crosapi {

// A `DeviceOwnershipWaiter` for tests that immediately executes the
// callback without actually waiting for the owner.
class FakeDeviceOwnershipWaiter : public DeviceOwnershipWaiter {
 public:
  FakeDeviceOwnershipWaiter() = default;

  FakeDeviceOwnershipWaiter(const FakeDeviceOwnershipWaiter&) = delete;
  FakeDeviceOwnershipWaiter& operator=(const FakeDeviceOwnershipWaiter&) =
      delete;

  ~FakeDeviceOwnershipWaiter() override = default;

  // `DeviceOwnershipWaiter`:
  void WaitForOwnershipFetched(base::OnceClosure callback,
                               bool launching_at_login_screen) override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FAKE_DEVICE_OWNERSHIP_WAITER_H_

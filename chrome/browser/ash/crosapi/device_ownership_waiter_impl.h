// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DEVICE_OWNERSHIP_WAITER_IMPL_H_
#define CHROME_BROWSER_ASH_CROSAPI_DEVICE_OWNERSHIP_WAITER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/crosapi/device_ownership_waiter.h"

namespace crosapi {

class DeviceOwnershipWaiterImpl : public DeviceOwnershipWaiter {
 public:
  DeviceOwnershipWaiterImpl() = default;
  DeviceOwnershipWaiterImpl(const DeviceOwnershipWaiterImpl&) = delete;
  DeviceOwnershipWaiterImpl& operator=(const DeviceOwnershipWaiterImpl&) =
      delete;
  ~DeviceOwnershipWaiterImpl() override = default;

  void WaitForOwnerhipFetched(base::OnceClosure callback,
                              bool launching_at_login_screen) override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DEVICE_OWNERSHIP_WAITER_IMPL_H_

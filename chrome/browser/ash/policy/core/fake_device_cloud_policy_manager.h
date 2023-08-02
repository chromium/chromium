// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_FAKE_DEVICE_CLOUD_POLICY_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_FAKE_DEVICE_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class DeviceCloudPolicyStoreAsh;

class FakeDeviceCloudPolicyManager : public DeviceCloudPolicyManagerAsh {
 public:
  FakeDeviceCloudPolicyManager(
      std::unique_ptr<DeviceCloudPolicyStoreAsh> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  FakeDeviceCloudPolicyManager(const FakeDeviceCloudPolicyManager&) = delete;
  FakeDeviceCloudPolicyManager& operator=(const FakeDeviceCloudPolicyManager&) =
      delete;

  ~FakeDeviceCloudPolicyManager() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_FAKE_DEVICE_CLOUD_POLICY_MANAGER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class DeviceCloudPolicyStoreChromeOS;

class FakeDeviceCloudPolicyManager : public DeviceCloudPolicyManagerChromeOS {
 public:
  FakeDeviceCloudPolicyManager(
      std::unique_ptr<DeviceCloudPolicyStoreChromeOS> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~FakeDeviceCloudPolicyManager() override;

  void set_unregister_result(bool value) {
    unregister_result_ = value;
  }

  // DeviceCloudPolicyManagerChromeOS:
  void Unregister(UnregisterCallback callback) override;
  void Disconnect() override;

 private:
  bool unregister_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceCloudPolicyManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_FAKE_DEVICE_CLOUD_POLICY_MANAGER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_CROS_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_CROS_TEST_HELPER_H_

#include <string>

#include "chrome/browser/ash/policy/core/device_policy_builder.h"

namespace policy {

class DevicePolicyCrosTestHelper {
 public:
  DevicePolicyCrosTestHelper();
  DevicePolicyCrosTestHelper(const DevicePolicyCrosTestHelper&) = delete;
  DevicePolicyCrosTestHelper& operator=(const DevicePolicyCrosTestHelper&) =
      delete;
  ~DevicePolicyCrosTestHelper();

  DevicePolicyBuilder* device_policy() { return &device_policy_; }
  const std::string device_policy_blob();

  // Writes the owner key to disk. To be called before installing a policy.
  void InstallOwnerKey();

  // Reinstalls |device_policy_| as the policy (to be used when it was
  // recently changed).
  void RefreshDevicePolicy();
  // Refreshes the Device Settings policies given in the settings vector.
  // Example: {chromeos::kDeviceDisplayResolution} refreshes the display
  //   resolution setting.
  void RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
      const std::vector<std::string>& settings);
  // Refreshes the whole device cloud policies.
  void RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();
  void UnsetPolicy(const std::vector<std::string>& settings);

 private:
  static void OverridePaths();

  // Carries Chrome OS device policies for tests.
  DevicePolicyBuilder device_policy_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_CROS_TEST_HELPER_H_

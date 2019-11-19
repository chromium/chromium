// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
class FakeSessionManagerClient;
}

namespace policy {

class DevicePolicyCrosTestHelper {
 public:
  DevicePolicyCrosTestHelper();
  ~DevicePolicyCrosTestHelper();

  // Writes the owner key to disk. To be called before installing a policy.
  void InstallOwnerKey();

  DevicePolicyBuilder* device_policy() { return &device_policy_; }

 private:
  static void OverridePaths();

  // Carries Chrome OS device policies for tests.
  DevicePolicyBuilder device_policy_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCrosTestHelper);
};

// Used to test Device policy changes in Chrome OS.
class DevicePolicyCrosBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  DevicePolicyCrosBrowserTest();
  ~DevicePolicyCrosBrowserTest() override;

  // Reinstalls |device_policy_| as the policy (to be used when it was
  // recently changed).
  void RefreshDevicePolicy();

  chromeos::DBusThreadManagerSetter* dbus_setter() {
    return dbus_setter_.get();
  }

  chromeos::FakeSessionManagerClient* session_manager_client() {
    return fake_session_manager_client_;
  }

  DevicePolicyBuilder* device_policy() { return test_helper_.device_policy(); }

  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

 private:
  DevicePolicyCrosTestHelper test_helper_;

  // FakeDBusThreadManager uses FakeSessionManagerClient.
  std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter_;
  chromeos::FakeSessionManagerClient* fake_session_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCrosBrowserTest);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_CROS_BROWSER_TEST_H_

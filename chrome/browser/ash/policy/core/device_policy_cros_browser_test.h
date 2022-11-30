// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_CROS_BROWSER_TEST_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_CROS_BROWSER_TEST_H_

#include <string>

#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {
class FakeSessionManagerClient;
}

namespace policy {

// Helpert functions for setting up device local account.
class DeviceLocalAccountTestHelper {
 public:
  static void SetupDeviceLocalAccount(UserPolicyBuilder* policy_builder,
                                      const std::string& kAccountId,
                                      const std::string& kDisplayName);

  static void AddPublicSession(
      enterprise_management::ChromeDeviceSettingsProto* proto,
      const std::string& kAccountId);
};

// Helper base class used for waiting for a pref value stored in local state
// to change to a particular expected value.
class LocalStateValueWaiter {
 public:
  LocalStateValueWaiter(const std::string& pref, base::Value expected_value);
  LocalStateValueWaiter(const LocalStateValueWaiter&) = delete;
  LocalStateValueWaiter& operator=(const LocalStateValueWaiter&) = delete;

  virtual bool ExpectedValueFound();
  virtual ~LocalStateValueWaiter();

  void Wait();

 protected:
  const std::string pref_;
  const base::Value expected_value_;

  PrefChangeRegistrar pref_change_registrar_;

 private:
  base::RunLoop run_loop_;
  void QuitLoopIfExpectedValueFound();
};

class DictionaryLocalStateValueWaiter : public LocalStateValueWaiter {
 public:
  DictionaryLocalStateValueWaiter(const std::string& pref,
                                  const std::string& expected_value,
                                  const std::string& key);

  DictionaryLocalStateValueWaiter(const DictionaryLocalStateValueWaiter&) =
      delete;
  DictionaryLocalStateValueWaiter& operator=(
      const DictionaryLocalStateValueWaiter&) = delete;
  ~DictionaryLocalStateValueWaiter() override;

 private:
  bool ExpectedValueFound() override;

  const std::string key_;
};

// Used to test Device policy changes in Chrome OS.
class DevicePolicyCrosBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  DevicePolicyCrosBrowserTest();
  DevicePolicyCrosBrowserTest(const DevicePolicyCrosBrowserTest&) = delete;
  DevicePolicyCrosBrowserTest& operator=(const DevicePolicyCrosBrowserTest&) =
      delete;
  ~DevicePolicyCrosBrowserTest() override;

  void RefreshDevicePolicy() { policy_helper()->RefreshDevicePolicy(); }

  DevicePolicyBuilder* device_policy() {
    return policy_helper()->device_policy();
  }

  ash::FakeSessionManagerClient* session_manager_client();

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  DevicePolicyCrosTestHelper* policy_helper() { return &policy_helper_; }

 private:
  DevicePolicyCrosTestHelper policy_helper_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_CROS_BROWSER_TEST_H_

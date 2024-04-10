// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace em = ::enterprise_management;

class DeviceSystemWideTracingEnabledPolicyTest
    : public DevicePolicyCrosBrowserTest {
 protected:
  DeviceSystemWideTracingEnabledPolicyTest() = default;
  ~DeviceSystemWideTracingEnabledPolicyTest() override = default;

  void OnPreferenceChanged(const std::string& pref_name) {
    if (run_loop_)
      run_loop_->Quit();
  }

  // Updates the device policy in |device_system_wide_tracing_enabled.enabled|.
  void UpdatePolicy(bool device_system_wide_tracing_enabled) {
    DevicePolicyBuilder* builder = device_policy();
    ASSERT_TRUE(builder);
    em::ChromeDeviceSettingsProto& proto(builder->payload());
    proto.mutable_device_system_wide_tracing_enabled()->set_enabled(
        device_system_wide_tracing_enabled);
  }

  // Refreshes device policy and waits for it to be applied.
  void SyncRefreshDevicePolicy() {
    PrefChangeRegistrar pref_change_registrar;
    pref_change_registrar.Init(g_browser_process->local_state());
    base::RepeatingCallback<void(const std::string&)> pref_changed_callback =
        base::BindRepeating(
            &DeviceSystemWideTracingEnabledPolicyTest::OnPreferenceChanged,
            base::Unretained(this));
    pref_change_registrar.Add(ash::prefs::kDeviceSystemWideTracingEnabled,
                              pref_changed_callback);

    run_loop_ = std::make_unique<base::RunLoop>();

    RefreshDevicePolicy();
    run_loop_->Run();

    run_loop_.reset();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
};

class DeviceSystemWideTracingEnabledPolicyConsumerOwnedTest
    : public DeviceSystemWideTracingEnabledPolicyTest {
 protected:
  DeviceSystemWideTracingEnabledPolicyConsumerOwnedTest()
      : install_attributes_(ash::StubInstallAttributes::CreateConsumerOwned()) {
  }
  ~DeviceSystemWideTracingEnabledPolicyConsumerOwnedTest() override = default;

  ash::ScopedStubInstallAttributes install_attributes_;
};

// Test that system-wide tracing is enabled by default for a consumer-owned
// device.
IN_PROC_BROWSER_TEST_F(DeviceSystemWideTracingEnabledPolicyConsumerOwnedTest,
                       DefaultEnabled) {
  ASSERT_TRUE(ChromeTracingDelegate::IsSystemWideTracingEnabled());
}

class DeviceSystemWideTracingEnabledPolicyEnterpriseManagedTest
    : public DeviceSystemWideTracingEnabledPolicyTest {
 protected:
  DeviceSystemWideTracingEnabledPolicyEnterpriseManagedTest()
      : install_attributes_(
            ash::StubInstallAttributes::CreateCloudManaged("fake-domain.com",
                                                           "fake-id")) {}
  ~DeviceSystemWideTracingEnabledPolicyEnterpriseManagedTest() override =
      default;

  ash::ScopedStubInstallAttributes install_attributes_;
};

// Test that system-wide tracing is disabled by default for a managed device and
// can be turned on by the policy.
IN_PROC_BROWSER_TEST_F(
    DeviceSystemWideTracingEnabledPolicyEnterpriseManagedTest,
    PolicyApplied) {
  ASSERT_FALSE(ChromeTracingDelegate::IsSystemWideTracingEnabled());

  UpdatePolicy(true);
  SyncRefreshDevicePolicy();
  ASSERT_TRUE(ChromeTracingDelegate::IsSystemWideTracingEnabled());

  UpdatePolicy(false);
  SyncRefreshDevicePolicy();
  ASSERT_FALSE(ChromeTracingDelegate::IsSystemWideTracingEnabled());
}

}  // namespace policy

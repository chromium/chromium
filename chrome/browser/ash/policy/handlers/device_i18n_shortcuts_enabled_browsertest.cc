// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"

namespace policy {

namespace em = ::enterprise_management;

class DeviceI18nShortcutsEnabledPolicyTest
    : public DevicePolicyCrosBrowserTest {
 protected:
  DeviceI18nShortcutsEnabledPolicyTest() = default;
  ~DeviceI18nShortcutsEnabledPolicyTest() override = default;

  void OnPreferenceChanged(const std::string& pref_name) {
    if (run_loop_)
      run_loop_->Quit();
  }

  void UpdatePolicy(bool device_i18n_shortcuts_enabled) {
    DevicePolicyBuilder* builder = device_policy();
    ASSERT_TRUE(builder);
    em::ChromeDeviceSettingsProto& proto(builder->payload());
    proto.mutable_device_i18n_shortcuts_enabled()->set_enabled(
        device_i18n_shortcuts_enabled);
  }

  // Refreshes device policy and waits for it to be applied.
  void SyncRefreshDevicePolicy() {
    PrefChangeRegistrar pref_change_registrar;
    pref_change_registrar.Init(g_browser_process->local_state());
    base::RepeatingCallback<void(const std::string&)> pref_changed_callback =
        base::BindRepeating(
            &DeviceI18nShortcutsEnabledPolicyTest::OnPreferenceChanged,
            base::Unretained(this));
    pref_change_registrar.Add(ash::prefs::kDeviceI18nShortcutsEnabled,
                              pref_changed_callback);

    run_loop_ = std::make_unique<base::RunLoop>();

    RefreshDevicePolicy();
    run_loop_->Run();

    run_loop_.reset();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
};

class DeviceI18nShortcutsEnabledPolicyEnterpriseManagedTest
    : public DeviceI18nShortcutsEnabledPolicyTest {
 protected:
  DeviceI18nShortcutsEnabledPolicyEnterpriseManagedTest()
      : install_attributes_(
            ash::StubInstallAttributes::CreateCloudManaged("fake-domain.com",
                                                           "fake-id")) {}
  ~DeviceI18nShortcutsEnabledPolicyEnterpriseManagedTest() override = default;

  ash::ScopedStubInstallAttributes install_attributes_;
};

IN_PROC_BROWSER_TEST_F(DeviceI18nShortcutsEnabledPolicyEnterpriseManagedTest,
                       PolicyApplied) {
  ASSERT_TRUE(features::IsImprovedKeyboardShortcutsEnabled());

  UpdatePolicy(false);
  SyncRefreshDevicePolicy();
  ASSERT_FALSE(features::IsImprovedKeyboardShortcutsEnabled());

  UpdatePolicy(true);
  SyncRefreshDevicePolicy();
  ASSERT_TRUE(features::IsImprovedKeyboardShortcutsEnabled());
}

}  // namespace policy

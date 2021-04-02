// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"

#include <stdint.h>

#include <string>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/prefs/pref_service.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace em = enterprise_management;

namespace policy {

void DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
    UserPolicyBuilder* policy_builder,
    const std::string& kAccountId,
    const std::string& kDisplayName) {
  policy_builder->policy_data().set_policy_type(
      policy::dm_protocol::kChromePublicAccountPolicyType);
  policy_builder->policy_data().set_username(kAccountId);
  policy_builder->policy_data().set_settings_entity_id(kAccountId);
  policy_builder->policy_data().set_public_key_version(1);
  policy_builder->payload().mutable_userdisplayname()->set_value(kDisplayName);
  policy_builder->payload()
      .mutable_devicelocalaccountmanagedsessionenabled()
      ->set_value(true);
}

void DeviceLocalAccountTestHelper::AddPublicSession(
    em::ChromeDeviceSettingsProto* proto,
    const std::string& kAccountId) {
  proto->mutable_show_user_names()->set_show_user_names(true);
  em::DeviceLocalAccountInfoProto* account =
      proto->mutable_device_local_accounts()->add_account();
  account->set_account_id(kAccountId);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
}

LocalStateValueWaiter::LocalStateValueWaiter(const std::string& pref,
                                             base::Value expected_value)
    : pref_(pref), expected_value_(std::move(expected_value)) {
  pref_change_registrar_.Init(g_browser_process->local_state());
}

LocalStateValueWaiter::~LocalStateValueWaiter() {}

bool LocalStateValueWaiter::ExpectedValueFound() {
  const base::Value* pref_value =
      pref_change_registrar_.prefs()->Get(pref_.c_str());
  if (!pref_value) {
    // Can't use ASSERT_* in non-void functions so this is the next best
    // thing.
    ADD_FAILURE() << "Pref " << pref_ << " not found";
    return true;
  }
  return *pref_value == expected_value_;
}

void LocalStateValueWaiter::QuitLoopIfExpectedValueFound() {
  if (ExpectedValueFound())
    run_loop_.Quit();
}

void LocalStateValueWaiter::Wait() {
  pref_change_registrar_.Add(
      pref_.c_str(),
      base::BindRepeating(&LocalStateValueWaiter::QuitLoopIfExpectedValueFound,
                          base::Unretained(this)));
  // Necessary if the pref value changes before the run loop is run. It is
  // safe to call RunLoop::Quit before RunLoop::Run (in which case the call
  // to Run will do nothing).
  QuitLoopIfExpectedValueFound();
  run_loop_.Run();
}

DictionaryLocalStateValueWaiter::DictionaryLocalStateValueWaiter(
    const std::string& pref,
    const std::string& expected_value,
    const std::string& key)
    : LocalStateValueWaiter(pref, base::Value(expected_value)), key_(key) {}

DictionaryLocalStateValueWaiter::~DictionaryLocalStateValueWaiter() {}

bool DictionaryLocalStateValueWaiter::ExpectedValueFound() {
  const base::DictionaryValue* pref =
      pref_change_registrar_.prefs()->GetDictionary(pref_.c_str());
  if (!pref) {
    // Can't use ASSERT_* in non-void functions so this is the next best
    // thing.
    ADD_FAILURE() << "Pref " << pref_ << " not found";
    return true;
  }
  std::string actual_value;
  return (pref->GetStringWithoutPathExpansion(key_, &actual_value) &&
          actual_value == expected_value_.GetString());
}

DevicePolicyCrosTestHelper::DevicePolicyCrosTestHelper() {}

DevicePolicyCrosTestHelper::~DevicePolicyCrosTestHelper() {}

void DevicePolicyCrosTestHelper::InstallOwnerKey() {
  OverridePaths();

  base::FilePath owner_key_file;
  ASSERT_TRUE(base::PathService::Get(chromeos::dbus_paths::FILE_OWNER_KEY,
                                     &owner_key_file));
  std::string owner_key_bits = device_policy()->GetPublicSigningKeyAsString();
  ASSERT_FALSE(owner_key_bits.empty());
  ASSERT_EQ(base::checked_cast<int>(owner_key_bits.length()),
            base::WriteFile(owner_key_file, owner_key_bits.data(),
                            owner_key_bits.length()));
}

// static
void DevicePolicyCrosTestHelper::OverridePaths() {
  // This is usually done by ChromeBrowserMainChromeOS, but some tests
  // use the overridden paths before ChromeBrowserMain starts. Make sure that
  // the paths are overridden before using them.
  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::ScopedAllowBlockingForTesting allow_io;
  chromeos::RegisterStubPathOverrides(user_data_dir);
  chromeos::dbus_paths::RegisterStubPathOverrides(user_data_dir);
}

const std::string DevicePolicyCrosTestHelper::device_policy_blob() {
  // Reset the key to its original state.
  device_policy()->SetDefaultSigningKey();
  device_policy()->Build();
  return device_policy()->GetBlob();
}

void DevicePolicyCrosTestHelper::RefreshDevicePolicy() {
  chromeos::FakeSessionManagerClient::Get()->set_device_policy(
      device_policy_blob());
  chromeos::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
}

void DevicePolicyCrosTestHelper::RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
    const std::vector<std::string>& settings) {
  base::RunLoop run_loop;

  // For calls from SetPolicy().
  std::vector<base::CallbackListSubscription> subscriptions = {};
  for (auto setting_it = settings.cbegin(); setting_it != settings.cend();
       setting_it++) {
    subscriptions.push_back(ash::CrosSettings::Get()->AddSettingsObserver(
        *setting_it, run_loop.QuitClosure()));
  }
  RefreshDevicePolicy();
  run_loop.Run();
  // Allow tasks posted by CrosSettings observers to complete:
  base::RunLoop().RunUntilIdle();
}

void DevicePolicyCrosTestHelper::UnsetPolicy(
    const std::vector<std::string>& settings) {
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.clear_display_rotation_default();
  proto.clear_device_display_resolution();
  RefreshPolicyAndWaitUntilDeviceSettingsUpdated(settings);
}

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest() {}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() = default;

chromeos::FakeSessionManagerClient*
DevicePolicyCrosBrowserTest::session_manager_client() {
  return chromeos::FakeSessionManagerClient::Get();
}

}  // namespace policy

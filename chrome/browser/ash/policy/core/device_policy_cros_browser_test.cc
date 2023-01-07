// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"

#include <stdint.h>

#include <string>

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

namespace em = ::enterprise_management;

}  // namespace

void DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
    UserPolicyBuilder* policy_builder,
    const std::string& kAccountId,
    const std::string& kDisplayName) {
  policy_builder->policy_data().set_policy_type(
      dm_protocol::kChromePublicAccountPolicyType);
  policy_builder->policy_data().set_username(kAccountId);
  policy_builder->policy_data().set_settings_entity_id(kAccountId);
  policy_builder->policy_data().set_public_key_version(1);
  policy_builder->payload().mutable_userdisplayname()->set_value(kDisplayName);
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
  const base::Value& pref_value =
      pref_change_registrar_.prefs()->GetValue(pref_.c_str());

  return pref_value == expected_value_;
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
  const base::Value::Dict& pref =
      pref_change_registrar_.prefs()->GetDict(pref_.c_str());

  const std::string* actual_value = pref.FindString(key_);
  return actual_value && *actual_value == expected_value_.GetString();
}

DevicePolicyCrosBrowserTest::DevicePolicyCrosBrowserTest() {}

DevicePolicyCrosBrowserTest::~DevicePolicyCrosBrowserTest() = default;

ash::FakeSessionManagerClient*
DevicePolicyCrosBrowserTest::session_manager_client() {
  return ash::FakeSessionManagerClient::Get();
}

}  // namespace policy

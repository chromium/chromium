// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/managed_guest_session_mixin.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace em = enterprise_management;

namespace {
constexpr char kPolicyAccountId[] = "managed-guest-session@localhost";
constexpr char kMgsDisplayName[] = "MGS";
}  // anonymous namespace

namespace ash {

ManagedGuestSessionMixin::ManagedGuestSessionMixin(
    InProcessBrowserTestMixinHost* host)
    : account_id_{AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kPolicyAccountId,
          policy::DeviceLocalAccountType::kPublicSession))},
      policy_test_server_mixin_(host),
      device_state_(
          host,
          ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED) {}

ManagedGuestSessionMixin::~ManagedGuestSessionMixin() = default;

void ManagedGuestSessionMixin::ConfigurePolicies() {
  AddManagedGuestSessionToDevicePolicy();
  SetUpDeviceLocalAccountPolicy();
}

void ManagedGuestSessionMixin::AddManagedGuestSessionToDevicePolicy() {
  policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
      &device_local_account_policy_, kPolicyAccountId, kMgsDisplayName);

  em::ChromeDeviceSettingsProto& proto =
      policy_helper_.device_policy()->payload();
  policy::DeviceLocalAccountTestHelper::AddPublicSession(&proto,
                                                         kPolicyAccountId);
  policy_helper_.RefreshDevicePolicy();
  policy_test_server_mixin_.UpdateDevicePolicy(proto);
}

void ManagedGuestSessionMixin::SetUpDeviceLocalAccountPolicy() {
  // Build device local account policy.
  device_local_account_policy_.SetDefaultSigningKey();
  device_local_account_policy_.Build();

  policy_test_server_mixin_.UpdatePolicy(
      policy::dm_protocol::kChromePublicAccountPolicyType, kPolicyAccountId,
      device_local_account_policy_.payload().SerializeAsString());

  ash::FakeSessionManagerClient::Get()->set_device_local_account_policy(
      kPolicyAccountId, device_local_account_policy_.GetBlob());
  policy_applied_ = true;

  // Wait for the display name to become available. That indicates the
  // device local account policy has loaded.
  policy::DictionaryLocalStateValueWaiter("UserDisplayName", kMgsDisplayName,
                                          account_id_.GetUserEmail())
      .Wait();
}

}  // namespace ash

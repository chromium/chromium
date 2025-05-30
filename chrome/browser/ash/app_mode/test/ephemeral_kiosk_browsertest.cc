// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CachePolicy;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;
using KioskEphemeralMode = policy::DeviceLocalAccount::EphemeralMode;

namespace {

// Possible values of the `EphemeralUsersEnabled` policy.
enum class DeviceEphemeralUsersPolicy { kUnset, kEnabled, kDisabled };

// Whether the Kiosk session should be ephemeral based on the given policies.
bool KioskShouldBeEphemeral(DeviceEphemeralUsersPolicy device_ephemeral,
                            KioskEphemeralMode kiosk_ephemeral) {
  switch (kiosk_ephemeral) {
    case KioskEphemeralMode::kUnset:
    case KioskEphemeralMode::kFollowDeviceWidePolicy:
      return device_ephemeral == DeviceEphemeralUsersPolicy::kEnabled;
    case KioskEphemeralMode::kDisable:
      return false;
    case KioskEphemeralMode::kEnable:
      return true;
  }
}

auto ToPolicyProtoValue(KioskEphemeralMode kiosk_ephemeral) {
  switch (kiosk_ephemeral) {
    case KioskEphemeralMode::kUnset:
      return enterprise_management::
          DeviceLocalAccountInfoProto_EphemeralMode_EPHEMERAL_MODE_UNSET;

    case KioskEphemeralMode::kFollowDeviceWidePolicy:
      return enterprise_management::
          DeviceLocalAccountInfoProto_EphemeralMode_EPHEMERAL_MODE_FOLLOW_DEVICE_WIDE_POLICY;

    case KioskEphemeralMode::kDisable:
      return enterprise_management::
          DeviceLocalAccountInfoProto_EphemeralMode_EPHEMERAL_MODE_DISABLE;

    case KioskEphemeralMode::kEnable:
      return enterprise_management::
          DeviceLocalAccountInfoProto_EphemeralMode_EPHEMERAL_MODE_ENABLE;
  }
}

// Sets the `DeviceEphemeralUsersEnabled` policy in `scoped_update`.
void UpdateDevicePolicy(ScopedDevicePolicyUpdate& scoped_update,
                        DeviceEphemeralUsersPolicy device_ephemeral) {
  if (device_ephemeral == DeviceEphemeralUsersPolicy::kUnset) {
    return;
  }

  scoped_update.policy_payload()
      ->mutable_ephemeral_users_enabled()
      ->set_ephemeral_users_enabled(device_ephemeral ==
                                    DeviceEphemeralUsersPolicy::kEnabled);
}

// Sets the `EphemeralMode` policy of device local accounts in `scoped_update`
// for the given `account_id`.
void UpdateDeviceLocalAccountPolicy(ScopedDevicePolicyUpdate& scoped_update,
                                    std::string_view account_id,
                                    KioskEphemeralMode kiosk_ephemeral) {
  if (kiosk_ephemeral == KioskEphemeralMode::kUnset) {
    return;
  }

  auto* accounts = scoped_update.policy_payload()
                       ->mutable_device_local_accounts()
                       ->mutable_account();
  auto account =
      std::ranges::find_if(*accounts, [&account_id](const auto& account) {
        return account.account_id() == account_id;
      });
  CHECK(account != accounts->end());
  account->set_ephemeral_mode(ToPolicyProtoValue(kiosk_ephemeral));
}

std::string_view ToString(DeviceEphemeralUsersPolicy device_ephemeral) {
  switch (device_ephemeral) {
    case DeviceEphemeralUsersPolicy::kUnset:
      return "Unset";
    case DeviceEphemeralUsersPolicy::kEnabled:
      return "Enabled";
    case DeviceEphemeralUsersPolicy::kDisabled:
      return "Disabled";
  }
}

std::string_view ToString(KioskEphemeralMode kiosk_ephemeral) {
  switch (kiosk_ephemeral) {
    case policy::DeviceLocalAccount::EphemeralMode::kUnset:
      return "Unset";
    case policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy:
      return "FollowDeviceWidePolicy";
    case policy::DeviceLocalAccount::EphemeralMode::kDisable:
      return "Disable";
    case policy::DeviceLocalAccount::EphemeralMode::kEnable:
      return "Enable";
  }
}

// Helper alias for the tuple used to combine test parameters.
using TestParam = std::
    tuple<KioskMixin::Config, DeviceEphemeralUsersPolicy, KioskEphemeralMode>;

// Returns the `TestParam` name to be used in gtest.
std::string ParamName(const testing::TestParamInfo<TestParam>& info) {
  auto [config, device_ephemeral, kiosk_ephemeral] = info.param;
  auto config_name =
      config.name.value_or(base::StringPrintf("%zu", info.index));
  return base::StrCat({config_name, "WithDeviceEphemeralUsers",
                       ToString(device_ephemeral), "AndKioskEphemeralMode",
                       ToString(kiosk_ephemeral)});
}

}  // namespace

class EphemeralKioskTest : public MixinBasedInProcessBrowserTest,
                           public testing::WithParamInterface<TestParam> {
 public:
  EphemeralKioskTest() = default;
  EphemeralKioskTest(const EphemeralKioskTest&) = delete;
  EphemeralKioskTest& operator=(const EphemeralKioskTest&) = delete;
  ~EphemeralKioskTest() override = default;

  DeviceEphemeralUsersPolicy DeviceEphemeralUsersParam() const {
    return std::get<DeviceEphemeralUsersPolicy>(GetParam());
  }

  KioskEphemeralMode KioskEphemeralModeParam() const {
    return std::get<KioskEphemeralMode>(GetParam());
  }

  const KioskMixin::Config& ConfigParam() const {
    return std::get<KioskMixin::Config>(GetParam());
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    std::string_view kiosk_account_id =
        ConfigParam().auto_launch_account_id.value().value();

    auto update = kiosk_.device_state_mixin().RequestDevicePolicyUpdate();
    UpdateDevicePolicy(*update, DeviceEphemeralUsersParam());
    UpdateDeviceLocalAccountPolicy(*update, kiosk_account_id,
                                   KioskEphemeralModeParam());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/ConfigParam()};
};

IN_PROC_BROWSER_TEST_P(EphemeralKioskTest,
                       KioskIsEphemeralWhenPoliciesConfigureIt) {
  bool kiosk_should_be_ephemeral = KioskShouldBeEphemeral(
      DeviceEphemeralUsersParam(), KioskEphemeralModeParam());

  EXPECT_EQ(
      kiosk_should_be_ephemeral,
      FakeUserDataAuthClient::TestApi::Get()->IsCurrentSessionEphemeral());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EphemeralKioskTest,
    testing::Combine(
        testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
        testing::Values(DeviceEphemeralUsersPolicy::kUnset,
                        DeviceEphemeralUsersPolicy::kEnabled,
                        DeviceEphemeralUsersPolicy::kDisabled),
        testing::Values(KioskEphemeralMode::kUnset,
                        KioskEphemeralMode::kFollowDeviceWidePolicy,
                        KioskEphemeralMode::kDisable,
                        KioskEphemeralMode::kEnable)),
    ParamName);

}  // namespace ash

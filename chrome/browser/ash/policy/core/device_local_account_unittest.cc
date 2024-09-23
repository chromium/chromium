// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kAccountId[] = "kiosk_account_id";
constexpr char kKioskAppId[] = "kiosk_app_id";

base::Value BuildDeviceLocalAccountsWithOneKioskAppWithEphemeralMode(
    DeviceLocalAccount::EphemeralMode ephemeral_mode) {
  return base::Value(base::Value::List().Append(
      base::Value::Dict()
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, kAccountId)
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
               static_cast<int>(DeviceLocalAccountType::kKioskApp))
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
               static_cast<int>(ephemeral_mode))
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
               kKioskAppId)));
}

}  // namespace

class DeviceLocalAccountTest : public testing::Test {
 public:
  DeviceLocalAccountTest() = default;

  DeviceLocalAccountTest(const DeviceLocalAccountTest&) = delete;
  DeviceLocalAccountTest& operator=(const DeviceLocalAccountTest&) = delete;

  ~DeviceLocalAccountTest() override = default;

 protected:
  void SetDeviceLocalAccountsPolicy(base::Value value) {
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kAccountsPrefDeviceLocalAccounts, std::move(value));
  }

 private:
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

TEST_F(DeviceLocalAccountTest, GetDeviceLocalAccountsValidEphemeral) {
  SetDeviceLocalAccountsPolicy(
      BuildDeviceLocalAccountsWithOneKioskAppWithEphemeralMode(
          DeviceLocalAccount::EphemeralMode::kEnable));

  const std::vector<DeviceLocalAccount> accounts =
      GetDeviceLocalAccounts(ash::CrosSettings::Get());
  ASSERT_EQ(accounts.size(), 1u);

  EXPECT_EQ(accounts[0].ephemeral_mode,
            DeviceLocalAccount::EphemeralMode::kEnable);
}

TEST_F(DeviceLocalAccountTest,
       GetDeviceLocalAccountsWithInvalidEphemeralModeShouldDefaultToUnset) {
  SetDeviceLocalAccountsPolicy(
      BuildDeviceLocalAccountsWithOneKioskAppWithEphemeralMode(
          static_cast<DeviceLocalAccount::EphemeralMode>(
              static_cast<int>(DeviceLocalAccount::EphemeralMode::kMaxValue) +
              1)));

  const std::vector<DeviceLocalAccount> accounts =
      GetDeviceLocalAccounts(ash::CrosSettings::Get());
  ASSERT_EQ(accounts.size(), 1u);

  EXPECT_EQ(accounts[0].ephemeral_mode,
            DeviceLocalAccount::EphemeralMode::kUnset);
}

TEST_F(DeviceLocalAccountTest,
       GetDeviceLocalAccountsWithMissingEphemeralModeShouldDefaultToUnset) {
  SetDeviceLocalAccountsPolicy(base::Value(base::Value::List().Append(
      base::Value::Dict()
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, kAccountId)
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
               static_cast<int>(DeviceLocalAccountType::kKioskApp))
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
               kKioskAppId))));

  const std::vector<DeviceLocalAccount> accounts =
      GetDeviceLocalAccounts(ash::CrosSettings::Get());
  ASSERT_EQ(accounts.size(), 1u);

  EXPECT_EQ(accounts[0].ephemeral_mode,
            DeviceLocalAccount::EphemeralMode::kUnset);
}

TEST_F(DeviceLocalAccountTest,
       GetDeviceLocalAccountsEphemeralModeShouldBeIgnoredForPublicSession) {
  SetDeviceLocalAccountsPolicy(base::Value(base::Value::List().Append(
      base::Value::Dict()
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId, kAccountId)
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
               static_cast<int>(DeviceLocalAccountType::kPublicSession))
          .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
               static_cast<int>(DeviceLocalAccount::EphemeralMode::kEnable)))));

  const std::vector<DeviceLocalAccount> accounts =
      GetDeviceLocalAccounts(ash::CrosSettings::Get());
  ASSERT_EQ(accounts.size(), 1u);

  EXPECT_EQ(accounts[0].ephemeral_mode,
            DeviceLocalAccount::EphemeralMode::kUnset);
}

}  // namespace policy

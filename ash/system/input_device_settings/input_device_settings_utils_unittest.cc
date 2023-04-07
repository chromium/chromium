// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include <cstdint>

#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/devices/input_device.h"

namespace ash {
namespace {

constexpr char kTestPrefKey[] = "test_key";
const AccountId account_id = AccountId::FromUserEmail("example@email.com");

}  // namespace

class DeviceKeyTest : public testing::TestWithParam<
                          std::tuple<uint16_t, uint16_t, std::string>> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    DeviceKeyTest,
    testing::ValuesIn(std::vector<std::tuple<uint16_t, uint16_t, std::string>>{
        {0x1234, 0x4321, "1234:4321"},
        {0xaaaa, 0xbbbb, "aaaa:bbbb"},
        {0xaa54, 0xffa1, "aa54:ffa1"},
        {0x1a2b, 0x3c4d, "1a2b:3c4d"},
        {0x5e6f, 0x7890, "5e6f:7890"},
        {0x0001, 0x0001, "0001:0001"},
        {0x1000, 0x1000, "1000:1000"}}));

TEST_P(DeviceKeyTest, BuildDeviceKey) {
  std::string expected_key;
  ui::InputDevice device;
  std::tie(device.vendor_id, device.product_id, expected_key) = GetParam();

  auto key = BuildDeviceKey(device);
  EXPECT_EQ(expected_key, key);
}

TEST(GetLoginScreenSettingsDictTest, RetrieveSettingsDict) {
  auto local_state = std::make_unique<TestingPrefServiceSimple>();
  user_manager::KnownUser::RegisterPrefs(local_state->registry());
  user_manager::KnownUser known_user(local_state.get());
  const base::Value::Dict* settings =
      GetLoginScreenSettingsDict(local_state.get(), account_id, kTestPrefKey);
  EXPECT_EQ(nullptr, settings);
  known_user.SetPath(account_id, kTestPrefKey,
                     absl::make_optional<base::Value>(base::Value::Dict()));
  const base::Value::Dict* valid_settings =
      GetLoginScreenSettingsDict(local_state.get(), account_id, kTestPrefKey);
  EXPECT_NE(nullptr, valid_settings);
}

}  // namespace ash

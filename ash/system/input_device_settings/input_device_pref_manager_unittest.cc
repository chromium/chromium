// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_pref_manager_impl.h"

#include <cstdint>

#include "ash/test/ash_test_base.h"
#include "ui/events/devices/input_device.h"

namespace ash {

class InputDevicePrefManagerTest : public AshTestBase {
 public:
  InputDevicePrefManagerTest() = default;
  InputDevicePrefManagerTest(const InputDevicePrefManagerTest&) = delete;
  InputDevicePrefManagerTest& operator=(const InputDevicePrefManagerTest&) =
      delete;
  ~InputDevicePrefManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<InputDevicePrefManagerImpl>();
  }

  void TearDown() override {
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDevicePrefManagerImpl> controller_;
};

TEST_F(InputDevicePrefManagerTest, InitializationTest) {
  EXPECT_NE(controller_.get(), nullptr);
}

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

  auto key = InputDevicePrefManagerImpl::BuildDeviceKey(device);
  EXPECT_EQ(expected_key, key);
}

}  // namespace ash

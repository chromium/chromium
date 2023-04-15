// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_key_alias_manager.h"

#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "ui/events/devices/input_device.h"

namespace ash {
namespace {
const ui::InputDevice CreateInputDevice(uint16_t vendor, uint16_t product) {
  return ui::InputDevice(5, ui::INPUT_DEVICE_INTERNAL, "kDeviceName", "",
                         base::FilePath(), vendor, product, 0);
}
}  // namespace
class InputDeviceKeyAliasManagerTest : public AshTestBase {
 public:
  InputDeviceKeyAliasManagerTest() = default;
  InputDeviceKeyAliasManagerTest(const InputDeviceKeyAliasManagerTest&) =
      delete;
  InputDeviceKeyAliasManagerTest& operator=(
      const InputDeviceKeyAliasManagerTest&) = delete;
  ~InputDeviceKeyAliasManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<InputDeviceKeyAliasManager>();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceKeyAliasManager> manager_;
};

TEST_F(InputDeviceKeyAliasManagerTest, DeviceKeyAliasing) {
  EXPECT_NE(manager_.get(), nullptr);
  const auto device_usb = CreateInputDevice(0x1111, 0x1111);
  const auto device_bluetooh = CreateInputDevice(0x1112, 0x1111);
  const auto primary_device_key = BuildDeviceKey(device_usb);
  const auto aliased_device_key = BuildDeviceKey(device_bluetooh);
  manager_->AddDeviceKeyPair(primary_device_key, aliased_device_key);

  EXPECT_EQ(primary_device_key, manager_->GetAliasedDeviceKey(device_usb));
  EXPECT_EQ(primary_device_key, manager_->GetAliasedDeviceKey(device_bluetooh));
  const auto aliases =
      *manager_->GetAliasesForPrimaryDeviceKey(primary_device_key);
  EXPECT_EQ(1u, aliases.size());
  EXPECT_TRUE(base::Contains(aliases, aliased_device_key));
}

}  // namespace ash

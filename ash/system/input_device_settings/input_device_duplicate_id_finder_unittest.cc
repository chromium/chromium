// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_duplicate_id_finder.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

namespace {

const ui::InputDevice CreateInputDevice(int id,
                                        uint16_t vendor,
                                        uint16_t product) {
  return ui::InputDevice(id, ui::INPUT_DEVICE_INTERNAL, "kDeviceName", "",
                         base::FilePath(), vendor, product, 0);
}

class TestObserver : public InputDeviceDuplicateIdFinder::Observer {
 public:
  // InputDeviceDuplicateIdFinder::Observer:
  void OnDuplicateDevicesUpdated() override { num_times_updated_++; }

  int num_times_updated() { return num_times_updated_; }

 private:
  int num_times_updated_ = 0;
};

}  // namespace

class InputDeviceDuplicateIdFinderTest : public AshTestBase {
 public:
  InputDeviceDuplicateIdFinderTest() = default;
  InputDeviceDuplicateIdFinderTest(const InputDeviceDuplicateIdFinderTest&) =
      delete;
  InputDeviceDuplicateIdFinderTest& operator=(
      const InputDeviceDuplicateIdFinderTest&) = delete;
  ~InputDeviceDuplicateIdFinderTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    // Duplicate ID finder should be created after the test base is setup.
    duplicate_id_finder_ = std::make_unique<InputDeviceDuplicateIdFinder>();
    observer_ = std::make_unique<TestObserver>();
    duplicate_id_finder_->AddObserver(observer_.get());
  }

  void TearDown() override {
    duplicate_id_finder_->RemoveObserver(observer_.get());
    observer_.reset();
    duplicate_id_finder_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceDuplicateIdFinder> duplicate_id_finder_;
  std::unique_ptr<TestObserver> observer_;
};

TEST_F(InputDeviceDuplicateIdFinderTest, DuplicateIdFinding) {
  auto duplicate_1_1 = CreateInputDevice(0, 0x1234, 0x4321);
  auto duplicate_1_2 = CreateInputDevice(1, 0x1234, 0x4321);
  auto duplicate_1_3 = CreateInputDevice(2, 0x1234, 0x4321);

  auto duplicate_2_1 = CreateInputDevice(3, 0x1234, 0x1234);
  auto duplicate_2_2 = CreateInputDevice(4, 0x1234, 0x1234);
  auto duplicate_2_3 = CreateInputDevice(5, 0x1234, 0x1234);

  auto duplicate_3_1 = CreateInputDevice(6, 0x4321, 0x5678);
  auto duplicate_3_2 = CreateInputDevice(7, 0x4321, 0x5678);
  auto duplicate_3_3 = CreateInputDevice(8, 0x4321, 0x5678);

  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {duplicate_1_1, duplicate_2_1, duplicate_3_1});
  EXPECT_EQ(1, observer_->num_times_updated());
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {duplicate_1_2, duplicate_2_2, duplicate_3_2});
  EXPECT_EQ(2, observer_->num_times_updated());
  ui::DeviceDataManagerTestApi().SetUncategorizedDevices({duplicate_1_3});
  EXPECT_EQ(3, observer_->num_times_updated());
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {ui::KeyboardDevice(duplicate_2_3)});
  EXPECT_EQ(4, observer_->num_times_updated());
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {ui::TouchpadDevice(duplicate_3_3)});
  EXPECT_EQ(5, observer_->num_times_updated());

  {
    auto* duplicate_group =
        duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_1_1.id);
    EXPECT_EQ(duplicate_group,
              duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_1_2.id));
    EXPECT_EQ(duplicate_group,
              duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_1_3.id));
    ASSERT_NE(nullptr, duplicate_group);
    EXPECT_TRUE(duplicate_group->contains(duplicate_1_1.id));
    EXPECT_TRUE(duplicate_group->contains(duplicate_1_2.id));
    EXPECT_TRUE(duplicate_group->contains(duplicate_1_3.id));
  }

  {
    auto* duplicate_group =
        duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_2_1.id);
    EXPECT_EQ(duplicate_group,
              duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_2_2.id));
    EXPECT_EQ(duplicate_group,
              duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_2_3.id));
    ASSERT_NE(nullptr, duplicate_group);
    EXPECT_TRUE(duplicate_group->contains(duplicate_2_1.id));
    EXPECT_TRUE(duplicate_group->contains(duplicate_2_2.id));
    EXPECT_TRUE(duplicate_group->contains(duplicate_2_3.id));
  }

  {
    auto* duplicate_group =
        duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_3_1.id);
    EXPECT_EQ(duplicate_group,
              duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_3_2.id));
    EXPECT_EQ(duplicate_group,
              duplicate_id_finder_->GetDuplicateDeviceIds(duplicate_3_3.id));
    ASSERT_NE(nullptr, duplicate_group);
    EXPECT_TRUE(duplicate_group->contains(duplicate_3_1.id));
    EXPECT_TRUE(duplicate_group->contains(duplicate_3_2.id));
    EXPECT_TRUE(duplicate_group->contains(duplicate_3_3.id));
  }
}

}  // namespace ash

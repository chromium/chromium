// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/hid_detection_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::hid_detection {
namespace {

using InputDeviceType = device::mojom::InputDeviceType;

enum HidType {
  kMouse,
  kTouchpad,
  kKeyboard,
};

}  // namespace

class HidDetectionManagerImplTest : public testing::Test {
 protected:
  HidDetectionManagerImplTest() = default;
  HidDetectionManagerImplTest(const HidDetectionManagerImplTest&) = delete;
  HidDetectionManagerImplTest& operator=(const HidDetectionManagerImplTest&) =
      delete;
  ~HidDetectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOobeHidDetectionRevamp);
    hid_detection_manager_ =
        std::make_unique<HidDetectionManagerImpl>(/*device_service=*/nullptr);

    HidDetectionManagerImpl::SetInputDeviceManagerBinderForTest(
        base::BindRepeating(&device::FakeInputServiceLinux::Bind,
                            base::Unretained(&fake_input_service_)));
  }

  void TearDown() override {
    HidDetectionManagerImpl::SetInputDeviceManagerBinderForTest(
        base::NullCallback());
  }

  absl::optional<bool> GetIsHidDetectionRequired() {
    absl::optional<bool> result;
    hid_detection_manager_->GetIsHidDetectionRequired(
        base::BindLambdaForTesting(
            [&result](bool is_required) { result = is_required; }));
    base::RunLoop().RunUntilIdle();
    return result;
  }

  void AddDevice(HidType hid_type, InputDeviceType device_type) {
    auto device = device::mojom::InputDeviceInfo::New();
    device->id = num_devices_created_++;
    device->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    device->type = device_type;
    switch (hid_type) {
      case kMouse:
        device->is_mouse = true;
        break;
      case kTouchpad:
        device->is_touchpad = true;
        break;
      case kKeyboard:
        device->is_keyboard = true;
        break;
    }
    fake_input_service_.AddDevice(std::move(device));
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  device::FakeInputServiceLinux fake_input_service_;

  size_t num_devices_created_ = 0;

  std::unique_ptr<hid_detection::HidDetectionManager> hid_detection_manager_;
};

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_NoDevicesConnected) {
  ASSERT_TRUE(GetIsHidDetectionRequired().has_value());
  EXPECT_TRUE(GetIsHidDetectionRequired().value());
}

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_OnlyPointerConnected) {
  AddDevice(HidType::kMouse, InputDeviceType::TYPE_USB);
  ASSERT_TRUE(GetIsHidDetectionRequired().has_value());
  EXPECT_TRUE(GetIsHidDetectionRequired().value());
}

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_OnlyKeyboardConnected) {
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_USB);
  ASSERT_TRUE(GetIsHidDetectionRequired().has_value());
  EXPECT_TRUE(GetIsHidDetectionRequired().value());
}

TEST_F(HidDetectionManagerImplTest,
       GetIsHidDetectionRequired_PointerAndKeyboardConnected) {
  AddDevice(HidType::kTouchpad, InputDeviceType::TYPE_USB);
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_USB);
  ASSERT_TRUE(GetIsHidDetectionRequired().has_value());
  EXPECT_FALSE(GetIsHidDetectionRequired().value());
}

}  // namespace ash::hid_detection

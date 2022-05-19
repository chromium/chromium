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
  kTouchscreen,
};

class FakeHidDetectionManagerDelegate : public HidDetectionManager::Delegate {
 public:
  ~FakeHidDetectionManagerDelegate() override = default;

  size_t num_hid_detection_status_changed_calls() const {
    return num_hid_detection_status_changed_calls_;
  }

  const absl::optional<HidDetectionManager::HidDetectionStatus>&
  last_hid_detection_status() const {
    return last_hid_detection_status_;
  }

 private:
  // HidDetectionManager::Delegate:
  void OnHidDetectionStatusChanged(
      HidDetectionManager::HidDetectionStatus status) override {
    ++num_hid_detection_status_changed_calls_;
    last_hid_detection_status_ = status;
  }

  size_t num_hid_detection_status_changed_calls_ = 0u;
  absl::optional<HidDetectionManager::HidDetectionStatus>
      last_hid_detection_status_;
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

  void StartHidDetection() {
    hid_detection_manager_->StartHidDetection(&delegate_);
    base::RunLoop().RunUntilIdle();
  }

  void StopHidDetection() {
    hid_detection_manager_->StopHidDetection();
    base::RunLoop().RunUntilIdle();
  }

  size_t GetNumHidDetectionStatusChangedCalls() {
    return delegate_.num_hid_detection_status_changed_calls();
  }

  const absl::optional<HidDetectionManager::HidDetectionStatus>&
  GetLastHidDetectionStatus() {
    return delegate_.last_hid_detection_status();
  }

  void AddDevice(HidType hid_type,
                 InputDeviceType device_type,
                 std::string* id_out = nullptr) {
    auto device = device::mojom::InputDeviceInfo::New();
    device->id = num_devices_created_++;
    if (id_out)
      *id_out = device->id;

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
      case kTouchscreen:
        device->is_touchscreen = true;
        break;
    }
    fake_input_service_.AddDevice(std::move(device));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveDevice(const std::string& id) {
    fake_input_service_.RemoveDevice(id);
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  device::FakeInputServiceLinux fake_input_service_;

  size_t num_devices_created_ = 0;

  FakeHidDetectionManagerDelegate delegate_;

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

TEST_F(HidDetectionManagerImplTest, StartDetection_TouchscreenPreConnected) {
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  EXPECT_TRUE(GetLastHidDetectionStatus()->touchscreen_detected);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_TouchscreenConnectedDisconnected) {
  AddDevice(HidType::kTouchpad, InputDeviceType::TYPE_USB);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  EXPECT_FALSE(GetLastHidDetectionStatus()->touchscreen_detected);

  // Add a non-touchscreen device. Touchscreen should not be detected.
  std::string device_id1;
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_USB, &device_id1);
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_FALSE(GetLastHidDetectionStatus()->touchscreen_detected);

  // Add touchscreen device. Touchscreen should be detected.
  std::string device_id2;
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO, &device_id2);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_TRUE(GetLastHidDetectionStatus()->touchscreen_detected);

  // Remove the non-touchscreen device. Touchscreen should still be detected.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_TRUE(GetLastHidDetectionStatus()->touchscreen_detected);

  // Remove the touchscreen device. Touchscreen should no longer be detected.
  RemoveDevice(device_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_FALSE(GetLastHidDetectionStatus()->touchscreen_detected);

  StopHidDetection();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_FALSE(GetLastHidDetectionStatus()->touchscreen_detected);

  // Add another touchscreen device. Delegate should not be notified.
  std::string device_id3;
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO, &device_id3);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_FALSE(GetLastHidDetectionStatus()->touchscreen_detected);

  // Remove the touchscreen device. Delegate should not be notified.
  RemoveDevice(device_id3);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_FALSE(GetLastHidDetectionStatus()->touchscreen_detected);
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_MultipleTouchscreensDisconnected) {
  std::string device_id1;
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO, &device_id1);
  std::string device_id2;
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO, &device_id2);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  EXPECT_TRUE(GetLastHidDetectionStatus()->touchscreen_detected);

  // Remove the first touchscreen device. The second touchscreen should be
  // detected and delegate notified.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  EXPECT_TRUE(GetLastHidDetectionStatus()->touchscreen_detected);
}

}  // namespace ash::hid_detection

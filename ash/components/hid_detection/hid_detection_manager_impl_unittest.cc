// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/hid_detection_manager_impl.h"

#include "ash/components/hid_detection/bluetooth_hid_detector.h"
#include "ash/components/hid_detection/fake_bluetooth_hid_detector.h"
#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::hid_detection {
namespace {

using BluetoothHidMetadata = BluetoothHidDetector::BluetoothHidMetadata;
using BluetoothHidType = BluetoothHidDetector::BluetoothHidType;
using InputMetadata = HidDetectionManager::InputMetadata;
using InputState = HidDetectionManager::InputState;
using InputDeviceType = device::mojom::InputDeviceType;
using InputDevicesStatus = BluetoothHidDetector::InputDevicesStatus;

const char kTestHidName[] = "testName";

enum HidType {
  kMouse,
  kTouchpad,
  kKeyboard,
  kTouchscreen,
  kTablet,
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
    fake_bluetooth_hid_detector_ = std::make_unique<FakeBluetoothHidDetector>();
    hid_detection_manager_ = std::make_unique<HidDetectionManagerImpl>(
        /*device_service=*/nullptr, fake_bluetooth_hid_detector_.get());

    HidDetectionManagerImpl::SetInputDeviceManagerBinderForTest(
        base::BindRepeating(&device::FakeInputServiceLinux::Bind,
                            base::Unretained(&fake_input_service_)));
  }

  void TearDown() override {
    if (fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active())
      StopHidDetection();

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
    EXPECT_FALSE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
    hid_detection_manager_->StartHidDetection(&delegate_);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
  }

  void StopHidDetection() {
    EXPECT_TRUE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
    hid_detection_manager_->StopHidDetection();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(
        fake_bluetooth_hid_detector_->is_bluetooth_hid_detection_active());
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
                 std::string* id_out = nullptr,
                 const char* name = NULL) {
    AddDevice(std::vector{hid_type}, device_type, id_out, name);
  }

  void AddDevice(std::vector<HidType> hid_types,
                 InputDeviceType device_type,
                 std::string* id_out = nullptr,
                 const char* name = NULL) {
    auto device = device::mojom::InputDeviceInfo::New();
    device->id = num_devices_created_++;
    if (id_out)
      *id_out = device->id;

    device->name = name == NULL ? device->id : name;
    device->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    device->type = device_type;

    for (const auto& hid_type : hid_types) {
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
        case kTablet:
          device->is_tablet = true;
          break;
      }
    }
    fake_input_service_.AddDevice(std::move(device));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveDevice(const std::string& id) {
    fake_input_service_.RemoveDevice(id);
    base::RunLoop().RunUntilIdle();
  }

  void SimulatePairingStarted(
      BluetoothHidDetector::BluetoothHidMetadata pairing_device) {
    fake_bluetooth_hid_detector_->SimulatePairingStarted(
        std::move(pairing_device));
    base::RunLoop().RunUntilIdle();
  }

  void SimulatePairingFinished() {
    fake_bluetooth_hid_detector_->SimulatePairingFinished();
    base::RunLoop().RunUntilIdle();
  }

  void AssertHidDetectionStatus(InputMetadata pointer_metadata,
                                InputMetadata keyboard_metadata,
                                bool touchscreen_detected) {
    EXPECT_EQ(pointer_metadata.state,
              GetLastHidDetectionStatus()->pointer_metadata.state);
    EXPECT_EQ(pointer_metadata.detected_hid_name,
              GetLastHidDetectionStatus()->pointer_metadata.detected_hid_name);
    EXPECT_EQ(keyboard_metadata.state,
              GetLastHidDetectionStatus()->keyboard_metadata.state);
    EXPECT_EQ(keyboard_metadata.detected_hid_name,
              GetLastHidDetectionStatus()->keyboard_metadata.detected_hid_name);
    EXPECT_EQ(touchscreen_detected,
              GetLastHidDetectionStatus()->touchscreen_detected);
  }

  void AssertInputDevicesStatus(InputDevicesStatus input_devices_status) {
    EXPECT_EQ(input_devices_status.pointer_is_missing,
              fake_bluetooth_hid_detector_->input_devices_status()
                  .pointer_is_missing);
    EXPECT_EQ(input_devices_status.keyboard_is_missing,
              fake_bluetooth_hid_detector_->input_devices_status()
                  .keyboard_is_missing);
  }

  size_t GetNumSetInputDevicesStatusCalls() {
    return fake_bluetooth_hid_detector_->num_set_input_devices_status_calls();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  device::FakeInputServiceLinux fake_input_service_;

  size_t num_devices_created_ = 0;

  FakeHidDetectionManagerDelegate delegate_;
  std::unique_ptr<FakeBluetoothHidDetector> fake_bluetooth_hid_detector_;

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
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest, StartDetection_PointerPreConnected) {
  std::string device_id;
  AddDevice(HidType::kMouse, InputDeviceType::TYPE_SERIO, &device_id);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnected, device_id},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest, StartDetection_KeyboardPreConnected) {
  std::string device_id;
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_SERIO, &device_id);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kConnected, device_id},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_TouchscreenConnectedDisconnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  std::string touchscreen_id1;
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO,
            &touchscreen_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  RemoveDevice(touchscreen_id1);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  StopHidDetection();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Add another touchscreen device. This should not inform the delegate.
  std::string touchscreen_id2;
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO,
            &touchscreen_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Remove the touchscreen device. This should not inform the delegate.
  RemoveDevice(touchscreen_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_PointerConnectedDisconnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  std::string pointer_id1;
  AddDevice(HidType::kMouse, InputDeviceType::TYPE_USB, &pointer_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, pointer_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  RemoveDevice(pointer_id1);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  StopHidDetection();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Add another pointer device. This should not inform the delegate.
  std::string pointer_id2;
  AddDevice(HidType::kMouse, InputDeviceType::TYPE_USB, &pointer_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Remove the pointer device. This should not inform the delegate.
  RemoveDevice(pointer_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_KeyboardConnectedDisconnected) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  std::string keyboard_id1;
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_USB, &keyboard_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kConnectedViaUsb, keyboard_id1},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});

  RemoveDevice(keyboard_id1);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  StopHidDetection();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Add another keyboard device. This should not inform the delegate.
  std::string keyboard_id2;
  AddDevice(HidType::kMouse, InputDeviceType::TYPE_USB, &keyboard_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Remove the keyboard device. This should not inform the delegate.
  RemoveDevice(keyboard_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_MultipleTouchscreensDisconnected) {
  std::string device_id1;
  AddDevice(HidType::kTablet, InputDeviceType::TYPE_SERIO, &device_id1);
  std::string device_id2;
  AddDevice(HidType::kTouchscreen, InputDeviceType::TYPE_SERIO, &device_id2);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Remove the first touchscreen device. The second touchscreen should be
  // detected and delegate notified.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/true);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_MultiplePointersDisconnected) {
  std::string device_id1;
  AddDevice(HidType::kTouchpad, InputDeviceType::TYPE_UNKNOWN, &device_id1);
  std::string device_id2;
  AddDevice(HidType::kMouse, InputDeviceType::TYPE_SERIO, &device_id2);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnected, device_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  // Remove the first pointer. The second pointer should be detected and
  // delegate notified.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnected, device_id2},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_MultipleKeyboardsDisconnected) {
  std::string device_id1;
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_UNKNOWN, &device_id1);
  std::string device_id2;
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_SERIO, &device_id2);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kConnected, device_id1},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});

  // Remove the first keyboard. The second keyboard should be detected and
  // delegate notified.
  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kConnected, device_id2},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_DeviceMultipleHidTypesDisconnected) {
  std::string device_id1;
  AddDevice(HidType::kTouchpad, InputDeviceType::TYPE_USB, &device_id1);
  std::string device_id2;
  std::vector<HidType> hid_types{HidType::kKeyboard, HidType::kTouchpad};
  AddDevice(hid_types, InputDeviceType::TYPE_SERIO, &device_id2);
  std::string device_id3;
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_UNKNOWN, &device_id3);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kConnected, device_id2},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  RemoveDevice(device_id1);
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnected, device_id2},
      /*keyboard_metadata=*/{InputState::kConnected, device_id2},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  RemoveDevice(device_id2);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kConnected, device_id3},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
}

// TODO(gordonseto): Test add device for type already connected, remove device
// for type already connected.

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothPointerSuccess) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kPointer});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  AddDevice(HidType::kMouse, InputDeviceType::TYPE_BLUETOOTH,
            /*id_out=*/nullptr, kTestHidName);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  SimulatePairingFinished();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothPointerFailure) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kPointer});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing failing.
  SimulatePairingFinished();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothKeyboardSuccess) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kKeyboard});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  AddDevice(HidType::kKeyboard, InputDeviceType::TYPE_BLUETOOTH,
            /*id_out=*/nullptr, kTestHidName);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});

  SimulatePairingFinished();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = false});
}

TEST_F(HidDetectionManagerImplTest, StartDetection_BluetoothKeyboardFailure) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(
      BluetoothHidMetadata{kTestHidName, BluetoothHidType::kKeyboard});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching, /*detected_hid_name=*/""},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing failing.
  SimulatePairingFinished();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_BluetoothKeyboardPointerComboSuccess) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(BluetoothHidMetadata{
      kTestHidName, BluetoothHidType::kKeyboardPointerCombo});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  AddDevice(std::vector{HidType::kKeyboard, HidType::kTouchpad},
            InputDeviceType::TYPE_BLUETOOTH,
            /*id_out=*/nullptr, kTestHidName);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  SimulatePairingFinished();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_BluetoothKeyboardComboFailure) {
  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  SimulatePairingStarted(BluetoothHidMetadata{
      kTestHidName, BluetoothHidType::kKeyboardPointerCombo});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});

  // Simulate the pairing failing.
  SimulatePairingFinished();
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kSearching,
                            /*detected_hid_name=*/""},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = true, .keyboard_is_missing = true});
}

TEST_F(HidDetectionManagerImplTest,
       StartDetection_BluetoothKeyboardPointerComboPointerPreConnected) {
  std::string device_id1;
  AddDevice(HidType::kTouchpad, InputDeviceType::TYPE_USB, &device_id1);
  EXPECT_EQ(0u, GetNumHidDetectionStatusChangedCalls());

  StartHidDetection();
  EXPECT_EQ(1u, GetNumHidDetectionStatusChangedCalls());
  ASSERT_TRUE(GetLastHidDetectionStatus().has_value());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  SimulatePairingStarted(BluetoothHidMetadata{
      kTestHidName, BluetoothHidType::kKeyboardPointerCombo});
  EXPECT_EQ(2u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kPairingViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(0u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});

  // Simulate the pairing succeeding.
  std::string device_id2;
  AddDevice(std::vector{HidType::kKeyboard, HidType::kTouchpad},
            InputDeviceType::TYPE_BLUETOOTH, &device_id2, kTestHidName);
  EXPECT_EQ(3u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  SimulatePairingFinished();
  EXPECT_EQ(4u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/{InputState::kPairedViaBluetooth, kTestHidName},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(1u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = false});

  // Disconnect the Bluetooth device.
  RemoveDevice(device_id2);
  EXPECT_EQ(5u, GetNumHidDetectionStatusChangedCalls());
  AssertHidDetectionStatus(
      /*pointer_metadata=*/{InputState::kConnectedViaUsb, device_id1},
      /*keyboard_metadata=*/
      {InputState::kSearching, /*detected_hid_name=*/""},
      /*touchscreen_detected=*/false);
  EXPECT_EQ(2u, GetNumSetInputDevicesStatusCalls());
  AssertInputDevicesStatus(
      {.pointer_is_missing = false, .keyboard_is_missing = true});
}

}  // namespace ash::hid_detection

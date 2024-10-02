// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestDeviceAddress1[] = "test_address";
constexpr char kValidModelId1[] = "abc";
constexpr char kDeviceDisplayName1[] = "test_nickname";
constexpr char kTestDeviceAddress2[] = "test_address2";
constexpr char kValidModelId2[] = "123";
constexpr char kDeviceDisplayName2[] = "test_nickname2";
}  // namespace

namespace ash {
namespace quick_pair {

class FastPairBluetoothConfigDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    // Enable feature.
    feature_list.InitAndEnableFeature(
        features::kFastPairDevicesBluetoothSettings);
    fast_pair_bluetooth_config_delegate_ =
        std::make_unique<FastPairBluetoothConfigDelegate>();
    fake_fast_pair_repository_ = std::make_unique<FakeFastPairRepository>();
  }

  scoped_refptr<Device> CreateDevice(const std::string& model_id,
                                     const std::string& device_address,
                                     std::string display_name) {
    scoped_refptr<Device> device = base::MakeRefCounted<Device>(
        model_id, device_address, Protocol::kFastPairSubsequent);
    device->set_display_name(display_name);
    return device;
  }

 protected:
  std::unique_ptr<FastPairBluetoothConfigDelegate>
      fast_pair_bluetooth_config_delegate_;
  std::unique_ptr<FakeFastPairRepository> fake_fast_pair_repository_;
  base::test::ScopedFeatureList feature_list;
};

TEST_F(FastPairBluetoothConfigDelegateTest,
       AddFastPairDeviceAddsDevicesToList) {
  // Checks that the initial list is empty.
  EXPECT_TRUE(
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties()
          .empty());

  // Adds new device to the list.
  fast_pair_bluetooth_config_delegate_->AddFastPairDevice(
      CreateDevice(kValidModelId1, kTestDeviceAddress1, kDeviceDisplayName1));

  // Gets updated list.
  std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
      fast_pairable_devices_properties =
          fast_pair_bluetooth_config_delegate_
              ->GetFastPairableDeviceProperties();

  // Checks that the GetFastPairableDeviceProperties gets the updated list.
  // TODO(b/294081604): Add FakeObserver to check that observers are being
  // notified and get the updated list.
  EXPECT_EQ(1u, fast_pairable_devices_properties.size());

  // Checks that the list is storing the right values.
  EXPECT_EQ(kTestDeviceAddress1,
            fast_pairable_devices_properties[0]->device_properties->address);
  EXPECT_EQ(kDeviceDisplayName1, fast_pairable_devices_properties[0]->nickname);
  EXPECT_EQ(
      bluetooth_config::mojom::FastPairableDevicePairingState::kReady,
      fast_pairable_devices_properties[0]->fast_pairable_device_pairing_state);
}

TEST_F(FastPairBluetoothConfigDelegateTest,
       RemoveFastPairDeviceRemovesSpecificDeviceFromList) {
  // Checks that the initial list is empty.
  EXPECT_TRUE(
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties()
          .empty());

  // Adds new device to the list.
  scoped_refptr<Device> mock_device1 =
      CreateDevice(kValidModelId1, kTestDeviceAddress1, kDeviceDisplayName1);
  fast_pair_bluetooth_config_delegate_->AddFastPairDevice(mock_device1);

  // Gets updated list.
  std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
      fast_pairable_devices_properties =
          fast_pair_bluetooth_config_delegate_
              ->GetFastPairableDeviceProperties();

  // Checks that the GetFastPairableDeviceProperties gets the updated list.
  // TODO(b/294081604): Add FakeObserver to check that observers are being
  // notified and get the updated list.
  EXPECT_EQ(1u, fast_pairable_devices_properties.size());

  // Checks that the list is storing the right values.
  EXPECT_EQ(kTestDeviceAddress1,
            fast_pairable_devices_properties[0]->device_properties->address);
  EXPECT_EQ(kDeviceDisplayName1, fast_pairable_devices_properties[0]->nickname);
  EXPECT_EQ(
      bluetooth_config::mojom::FastPairableDevicePairingState::kReady,
      fast_pairable_devices_properties[0]->fast_pairable_device_pairing_state);

  // Adds a second device to the list.
  fast_pair_bluetooth_config_delegate_->AddFastPairDevice(
      CreateDevice(kValidModelId2, kTestDeviceAddress2, kDeviceDisplayName2));

  // Gets updated list.
  fast_pairable_devices_properties =
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties();

  // Checks the list is updated.
  // TODO(b/294081604): Add FakeObserver to check that observers are being
  // notified and get the updated list.
  EXPECT_EQ(2u, fast_pairable_devices_properties.size());

  // Removes first device from the list.
  fast_pair_bluetooth_config_delegate_->RemoveFastPairDevice(mock_device1);
  fast_pairable_devices_properties =
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties();

  // Checks that the GetFastPairableDeviceProperties gets the updated list.
  // TODO(b/294081604): Add FakeObserver to check that observers are being
  // notified and get the updated list.
  EXPECT_EQ(1u, fast_pairable_devices_properties.size());

  // Checks that the list is storing the right values.
  EXPECT_EQ(kTestDeviceAddress2,
            fast_pairable_devices_properties[0]->device_properties->address);
  EXPECT_EQ(kDeviceDisplayName2, fast_pairable_devices_properties[0]->nickname);
  EXPECT_EQ(
      bluetooth_config::mojom::FastPairableDevicePairingState::kReady,
      fast_pairable_devices_properties[0]->fast_pairable_device_pairing_state);
}

TEST_F(FastPairBluetoothConfigDelegateTest,
       ClearFastPairableDevicesRemovesAllDevicesFromList) {
  // Checks that the initial list is empty.
  EXPECT_TRUE(
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties()
          .empty());

  // Adds two different devices to the list.
  fast_pair_bluetooth_config_delegate_->AddFastPairDevice(
      CreateDevice(kValidModelId1, kTestDeviceAddress1, kDeviceDisplayName1));
  fast_pair_bluetooth_config_delegate_->AddFastPairDevice(
      CreateDevice(kValidModelId2, kTestDeviceAddress2, kDeviceDisplayName2));

  // Checks that the list is updated.
  // TODO(b/294081604): Add FakeObserver to check that observers are being
  // notified and get the updated list.
  EXPECT_EQ(2u, fast_pair_bluetooth_config_delegate_
                    ->GetFastPairableDeviceProperties()
                    .size());

  // Clears list.
  fast_pair_bluetooth_config_delegate_->ClearFastPairableDevices();

  // Checks that the list is empty.
  EXPECT_TRUE(
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties()
          .empty());
}

TEST_F(FastPairBluetoothConfigDelegateTest,
       UpdateFastPairableDevicePairingStateChangesPairingState) {
  // Checks that the initial list is empty.
  EXPECT_TRUE(
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties()
          .empty());

  // Adds new device to the list.
  scoped_refptr<Device> mock_device1 =
      CreateDevice(kValidModelId1, kTestDeviceAddress1, kDeviceDisplayName1);
  fast_pair_bluetooth_config_delegate_->AddFastPairDevice(mock_device1);

  // Gets updated list.
  std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
      fast_pairable_devices_properties =
          fast_pair_bluetooth_config_delegate_
              ->GetFastPairableDeviceProperties();

  // Checks that the GetFastPairableDeviceProperties gets the updated lists.
  // TODO(b/294081604): Add FakeObserver to check that observers are being
  // notified and get the updated list.
  EXPECT_EQ(1u, fast_pairable_devices_properties.size());

  // Checks that the list is storing the right values.
  EXPECT_EQ(kTestDeviceAddress1,
            fast_pairable_devices_properties[0]->device_properties->address);
  EXPECT_EQ(kDeviceDisplayName1, fast_pairable_devices_properties[0]->nickname);
  EXPECT_EQ(
      bluetooth_config::mojom::FastPairableDevicePairingState::kReady,
      fast_pairable_devices_properties[0]->fast_pairable_device_pairing_state);

  // Updates the pairing state for mock_device1.
  fast_pair_bluetooth_config_delegate_->UpdateFastPairableDevicePairingState(
      mock_device1,
      bluetooth_config::mojom::FastPairableDevicePairingState::kError);

  // Gets updated list of device properties.
  fast_pairable_devices_properties =
      fast_pair_bluetooth_config_delegate_->GetFastPairableDeviceProperties();

  // Checks that the Pairing State has been updated for the specific device and
  // that the other values remain the same.
  EXPECT_EQ(kTestDeviceAddress1,
            fast_pairable_devices_properties[0]->device_properties->address);
  EXPECT_EQ(kDeviceDisplayName1, fast_pairable_devices_properties[0]->nickname);
  EXPECT_EQ(
      bluetooth_config::mojom::FastPairableDevicePairingState::kError,
      fast_pairable_devices_properties[0]->fast_pairable_device_pairing_state);
}

}  // namespace quick_pair
}  // namespace ash

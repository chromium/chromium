// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFirstSavedMacAddress[] = "00:11:22:33:44";
constexpr char kSecondSavedMacAddress[] = "AA:11:BB:33:CC";
constexpr char kNotSavedMacAddress[] = "FF:FF:FF:FF:FF";
const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
const std::vector<uint8_t> kAccountKey2{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                        0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                        0x77, 0x77, 0x88, 0x88};

}  // namespace

namespace ash {
namespace quick_pair {

class SavedDeviceRegistryTest : public testing::Test {
 public:
  SavedDeviceRegistryTest()
      : adapter_(base::MakeRefCounted<
                 testing::NiceMock<device::MockBluetoothAdapter>>()),
        bluetooth_device1_(/*adapter=*/adapter_.get(),
                           /*bluetooth_class=*/0,
                           /*name=*/"Test name 1",
                           /*address=*/kFirstSavedMacAddress,
                           /*initially_paired=*/true,
                           /*connected=*/true),
        bluetooth_device2_(/*adapter=*/adapter_.get(),
                           /*bluetooth_class=*/0,
                           /*name=*/"Test name 1",
                           /*address=*/kSecondSavedMacAddress,
                           /*initially_paired=*/true,
                           /*connected=*/true) {
    ON_CALL(bluetooth_device1_, IsPaired).WillByDefault(testing::Return(true));
    ON_CALL(bluetooth_device2_, IsPaired).WillByDefault(testing::Return(true));
    ON_CALL(*adapter_, GetDevices).WillByDefault(testing::Return(device_list_));
  }

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    SavedDeviceRegistry::RegisterProfilePrefs(pref_service_->registry());

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetActivePrefService())
        .WillByDefault(testing::Return(pref_service_.get()));

    saved_device_registry_ = std::make_unique<SavedDeviceRegistry>(adapter_);
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

 protected:
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  testing::NiceMock<device::MockBluetoothDevice> bluetooth_device1_;
  testing::NiceMock<device::MockBluetoothDevice> bluetooth_device2_;
  device::BluetoothAdapter::ConstDeviceList device_list_{&bluetooth_device1_,
                                                         &bluetooth_device2_};
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<SavedDeviceRegistry> saved_device_registry_;
};

TEST_F(SavedDeviceRegistryTest, ValidLookup) {
  bool success1 = saved_device_registry_->SaveAccountAssociation(
      kFirstSavedMacAddress, kAccountKey1);
  bool success2 = saved_device_registry_->SaveAccountAssociation(
      kSecondSavedMacAddress, kAccountKey2);

  auto first = saved_device_registry_->GetAccountKey(kFirstSavedMacAddress);
  auto second = saved_device_registry_->GetAccountKey(kSecondSavedMacAddress);

  ASSERT_EQ(kAccountKey1, *first);
  ASSERT_EQ(kAccountKey2, *second);

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);

  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));
}

TEST_F(SavedDeviceRegistryTest, InvalidLookup) {
  bool success1 = saved_device_registry_->SaveAccountAssociation(
      kFirstSavedMacAddress, kAccountKey1);

  auto invalid_result =
      saved_device_registry_->GetAccountKey(kNotSavedMacAddress);
  ASSERT_EQ(std::nullopt, invalid_result);

  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_TRUE(success1);
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));
}

TEST_F(SavedDeviceRegistryTest, MissingPrefService) {
  ON_CALL(*browser_delegate_, GetActivePrefService())
      .WillByDefault(testing::Return(nullptr));
  bool failure1 = saved_device_registry_->SaveAccountAssociation(
      kFirstSavedMacAddress, kAccountKey1);
  bool failure2 = saved_device_registry_->SaveAccountAssociation(
      kSecondSavedMacAddress, kAccountKey2);

  auto first = saved_device_registry_->GetAccountKey(kFirstSavedMacAddress);
  auto second = saved_device_registry_->GetAccountKey(kSecondSavedMacAddress);

  ASSERT_EQ(std::nullopt, first);
  ASSERT_EQ(std::nullopt, second);

  EXPECT_FALSE(saved_device_registry_->DeleteAccountKey(kFirstSavedMacAddress));
  EXPECT_FALSE(
      saved_device_registry_->DeleteAccountKey(kSecondSavedMacAddress));

  EXPECT_FALSE(failure1);
  EXPECT_FALSE(failure2);

  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));
}

TEST_F(SavedDeviceRegistryTest, DeleteAccountKey_MacAddress) {
  bool success1 = saved_device_registry_->SaveAccountAssociation(
      kFirstSavedMacAddress, kAccountKey1);
  bool success2 = saved_device_registry_->SaveAccountAssociation(
      kSecondSavedMacAddress, kAccountKey2);

  auto first = saved_device_registry_->GetAccountKey(kFirstSavedMacAddress);
  auto second = saved_device_registry_->GetAccountKey(kSecondSavedMacAddress);

  ASSERT_EQ(kAccountKey1, *first);
  ASSERT_EQ(kAccountKey2, *second);

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);

  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));

  // Remove the first account key.
  EXPECT_TRUE(saved_device_registry_->DeleteAccountKey(kFirstSavedMacAddress));
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));

  // Remove the second account key.
  EXPECT_TRUE(saved_device_registry_->DeleteAccountKey(kSecondSavedMacAddress));
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));

  // Removing a key that doesn't exist/is already removed should return false.
  EXPECT_FALSE(saved_device_registry_->DeleteAccountKey(kFirstSavedMacAddress));
}

TEST_F(SavedDeviceRegistryTest, DeleteAccountKey_AccountKey) {
  bool success1 = saved_device_registry_->SaveAccountAssociation(
      kFirstSavedMacAddress, kAccountKey1);
  bool success2 = saved_device_registry_->SaveAccountAssociation(
      kSecondSavedMacAddress, kAccountKey2);

  auto first = saved_device_registry_->GetAccountKey(kFirstSavedMacAddress);
  auto second = saved_device_registry_->GetAccountKey(kSecondSavedMacAddress);

  ASSERT_EQ(kAccountKey1, *first);
  ASSERT_EQ(kAccountKey2, *second);

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);

  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));

  // Remove the first account key.
  EXPECT_TRUE(saved_device_registry_->DeleteAccountKey(kAccountKey1));
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));

  // Remove the second account key.
  EXPECT_TRUE(saved_device_registry_->DeleteAccountKey(kAccountKey2));
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));

  // Removing a key that doesn't exist/is already removed should return false.
  EXPECT_FALSE(saved_device_registry_->DeleteAccountKey(kAccountKey1));
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_IsAccountKeySavedToRegistry_DeviceRemoved \
  DISABLED_IsAccountKeySavedToRegistry_DeviceRemoved
#else
#define MAYBE_IsAccountKeySavedToRegistry_DeviceRemoved \
  IsAccountKeySavedToRegistry_DeviceRemoved
#endif
TEST_F(SavedDeviceRegistryTest,
       MAYBE_IsAccountKeySavedToRegistry_DeviceRemoved) {
  // Simulate a user saving devices to their account.
  bool success1 = saved_device_registry_->SaveAccountAssociation(
      kFirstSavedMacAddress, kAccountKey1);
  bool success2 = saved_device_registry_->SaveAccountAssociation(
      kSecondSavedMacAddress, kAccountKey2);

  EXPECT_TRUE(success1);
  EXPECT_TRUE(success2);

  // Destroy the object to simulate a user's session ending.
  saved_device_registry_.reset();

  // Simulate a device being removed from the bluetooth adapter. This is meant
  // to replicate the circumstances of a second user forgetting a device.
  device::BluetoothAdapter::ConstDeviceList device_list{&bluetooth_device2_};
  ON_CALL(*adapter_, GetDevices()).WillByDefault(testing::Return(device_list));

  // Create a new object to simulate a new user session.
  saved_device_registry_ =
      std::make_unique<SavedDeviceRegistry>(adapter_.get());

  // We expect |kAccountKey1| to be removed from the registry since it is no
  // longer paired.
  EXPECT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));
}

}  // namespace quick_pair
}  // namespace ash

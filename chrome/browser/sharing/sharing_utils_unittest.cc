// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SharingUtilsTest : public testing::Test {
 public:
  SharingUtilsTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  syncer::TestSyncService test_sync_service_;
};

static std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& id,
    const std::string& name,
    sync_pb::SyncEnums_DeviceType device_type =
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
    base::SysInfo::HardwareInfo hardware_info = base::SysInfo::HardwareInfo()) {
  return std::make_unique<syncer::DeviceInfo>(
      id, name, "chrome_version", "user_agent", device_type, "device_id",
      hardware_info,
      /*last_updated_timestamp=*/base::Time::Now(),
      /*send_tab_to_self_receiving_enabled=*/false,
      syncer::DeviceInfo::SharingInfo(
          {"vapid_fcm_token", "vapid_p256dh", "vapid_auth_secret"},
          {"sender_id_fcm_token", "sender_id_p256dh", "sender_id_auth_secret"},
          std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
              sync_pb::SharingSpecificFields::CLICK_TO_CALL}));
}

}  // namespace

TEST_F(SharingUtilsTest, GetSharingDeviceNames_AppleDevices_SigninOnly) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "MacbookPro1,1", sync_pb::SyncEnums_DeviceType_TYPE_MAC,
      {"Apple Inc.", "MacbookPro1,1", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("MacbookPro1,1", names.full_name);
  EXPECT_EQ("MacbookPro", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_AppleDevices_FullySynced) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Bobs-iMac", sync_pb::SyncEnums_DeviceType_TYPE_MAC,
      {"Apple Inc.", "MacbookPro1,1", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Bobs-iMac", names.full_name);
  EXPECT_EQ("Bobs-iMac", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_ChromeOSDevices_FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kSharingRenameDevices);
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Chromebook", sync_pb::SyncEnums_DeviceType_TYPE_CROS,
      {"Google", "Chromebook", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Google Chromebook", names.full_name);
  EXPECT_EQ("Google Chromebook", names.short_name);
}

TEST_F(SharingUtilsTest,
       GetSharingDeviceNames_ChromeOSDevices_FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kSharingRenameDevices);
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Chromebook", sync_pb::SyncEnums_DeviceType_TYPE_CROS,
      {"Google", "Chromebook", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Chromebook", names.full_name);
  EXPECT_EQ("Chromebook", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_AndroidPhones_FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kSharingRenameDevices);
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel 2", sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
      {"Google", "Pixel 2", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Google Phone Pixel 2", names.full_name);
  EXPECT_EQ("Google Phone", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_AndroidPhones_FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kSharingRenameDevices);
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel 2", sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
      {"Google", "Pixel 2", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Pixel 2", names.full_name);
  EXPECT_EQ("Pixel 2", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_AndroidTablets_FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kSharingRenameDevices);
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel C", sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
      {"Google", "Pixel C", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Google Tablet Pixel C", names.full_name);
  EXPECT_EQ("Google Tablet", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_AndroidTablets_FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kSharingRenameDevices);
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel C", sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
      {"Google", "Pixel C", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Pixel C", names.full_name);
  EXPECT_EQ("Pixel C", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_Windows_SigninOnly) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BX123", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      {"Dell", "BX123", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Dell Computer BX123", names.full_name);
  EXPECT_EQ("Dell Computer", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_Windows_FullySynced) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BOBS-WINDOWS-1", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      {"Dell", "BX123", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("BOBS-WINDOWS-1", names.full_name);
  EXPECT_EQ("BOBS-WINDOWS-1", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_Linux_SigninOnly) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "30BDS0RA0G", sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      {"LENOVO", "30BDS0RA0G", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("LENOVO Computer 30BDS0RA0G", names.full_name);
  EXPECT_EQ("LENOVO Computer", names.short_name);
}

TEST_F(SharingUtilsTest, GetSharingDeviceNames_Linux_FullySynced) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "bob.chromium.org", sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      {"LENOVO", "30BDS0RA0G", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("bob.chromium.org", names.full_name);
  EXPECT_EQ("bob.chromium.org", names.short_name);
}

TEST_F(SharingUtilsTest, CheckManufacturerNameCapitalization) {
  std::unique_ptr<syncer::DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "model", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      {"foo bar", "model", ""});
  SharingDeviceNames names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Foo Bar Computer model", names.full_name);
  EXPECT_EQ("Foo Bar Computer", names.short_name);

  device = CreateFakeDeviceInfo("guid", "model",
                                sync_pb::SyncEnums_DeviceType_TYPE_WIN,
                                {"foo1bar", "model", ""});
  names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Foo1Bar Computer model", names.full_name);
  EXPECT_EQ("Foo1Bar Computer", names.short_name);

  device = CreateFakeDeviceInfo("guid", "model",
                                sync_pb::SyncEnums_DeviceType_TYPE_WIN,
                                {"foo_bar-FOO", "model", ""});
  names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Foo_Bar-FOO Computer model", names.full_name);
  EXPECT_EQ("Foo_Bar-FOO Computer", names.short_name);

  device = CreateFakeDeviceInfo("guid", "model",
                                sync_pb::SyncEnums_DeviceType_TYPE_WIN,
                                {"foo&bar foo", "model", ""});
  names = GetSharingDeviceNames(device.get());

  EXPECT_EQ("Foo&Bar Foo Computer model", names.full_name);
  EXPECT_EQ("Foo&Bar Foo Computer", names.short_name);
}

TEST_F(SharingUtilsTest, SyncEnabled_SigninOnly) {
  // Enable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey},
      /*disabled_features=*/{});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes({syncer::DEVICE_INFO});

  EXPECT_TRUE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncEnabled_FullySynced) {
  // Disable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_TRUE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_SigninOnly_MissingDataTypes) {
  // Enable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey},
      /*disabled_features=*/{});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes({});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_FullySynced_MissingDataTypes) {
  // Disable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes({syncer::DEVICE_INFO});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_Disabled) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_Configuring) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include <memory>

#include "base/guid.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/sharing/features.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kVapidKeyStr[] = "test_vapid_key";
const std::vector<uint8_t> kVapidKey =
    std::vector<uint8_t>(std::begin(kVapidKeyStr), std::end(kVapidKeyStr));

const char kDeviceGuid[] = "test_device";
const char kDeviceName[] = "test_name";
const char kDeviceVapidFcmToken[] = "test_vapid_fcm_token";
const char kDeviceVapidAuthToken[] = "test_vapid_auth_token";
const char kDeviceVapidP256dh[] = "test_vapid_p256dh";
const char kDeviceSenderIdFcmToken[] = "test_sender_id_fcm_token";
const char kDeviceSenderIdAuthToken[] = "test_sender_id_auth_token";
const char kDeviceSenderIdP256dh[] = "test_sender_id_p256dh";

const char kAuthorizedEntity[] = "authorized_entity";

void ExpectSharingInfoEquals(
    const base::Optional<syncer::DeviceInfo::SharingInfo>& sharing_info,
    const base::Optional<syncer::DeviceInfo::SharingTargetInfo>&
        vapid_target_info,
    const std::set<sync_pb::SharingSpecificFields::EnabledFeatures>&
        enabled_features) {
  ASSERT_TRUE(sharing_info);
  ASSERT_TRUE(vapid_target_info);
  EXPECT_EQ(sharing_info->vapid_target_info.fcm_token,
            vapid_target_info->fcm_token);
  EXPECT_EQ(sharing_info->vapid_target_info.p256dh, vapid_target_info->p256dh);
  EXPECT_EQ(sharing_info->vapid_target_info.auth_secret,
            vapid_target_info->auth_secret);
  EXPECT_EQ(sharing_info->enabled_features, enabled_features);
}

}  // namespace

class SharingSyncPreferenceTest : public testing::Test {
 protected:
  SharingSyncPreferenceTest()
      : sharing_sync_preference_(&prefs_, &fake_device_info_sync_service_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  syncer::DeviceInfo::SharingInfo GetDefaultSharingInfo() {
    return syncer::DeviceInfo::SharingInfo(
        {kDeviceVapidFcmToken, kDeviceVapidP256dh, kDeviceVapidAuthToken},
        {kDeviceSenderIdFcmToken, kDeviceSenderIdP256dh,
         kDeviceSenderIdAuthToken},
        std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
            sync_pb::SharingSpecificFields::CLICK_TO_CALL});
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sharing_sync_preference_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SharingSyncPreferenceTest, UpdateVapidKeys) {
  EXPECT_EQ(base::nullopt, sharing_sync_preference_.GetVapidKey());
  sharing_sync_preference_.SetVapidKey(kVapidKey);
  EXPECT_EQ(kVapidKey, sharing_sync_preference_.GetVapidKey());
}

TEST_F(SharingSyncPreferenceTest, SyncAndRemoveLocalDevice) {
  scoped_feature_list_.InitAndEnableFeature(kSharingUseDeviceInfo);
  const syncer::DeviceInfo* local_device_info =
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(local_device_info);
  EXPECT_FALSE(sharing_sync_preference_.GetLocalSharingInfo());
  EXPECT_FALSE(
      sharing_sync_preference_.GetTargetInfo(local_device_info->guid()));
  EXPECT_TRUE(
      sharing_sync_preference_.GetEnabledFeatures(local_device_info).empty());

  // Setting SharingInfo should trigger RefreshLocalDeviceInfoCount.
  auto sharing_info = GetDefaultSharingInfo();
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);

  EXPECT_EQ(sharing_info, sharing_sync_preference_.GetLocalSharingInfo());
  ExpectSharingInfoEquals(
      sharing_info,
      sharing_sync_preference_.GetTargetInfo(local_device_info->guid()),
      sharing_sync_preference_.GetEnabledFeatures(local_device_info));
  EXPECT_EQ(1, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());

  // Assume LocalDeviceInfoProvider is updated now.
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(sharing_info);

  // Setting exactly the same SharingInfo in LocalDeviceInfoProvider shouldn't
  // trigger RefreshLocalDeviceInfoCount.
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);

  EXPECT_EQ(sharing_info, sharing_sync_preference_.GetLocalSharingInfo());
  ExpectSharingInfoEquals(
      sharing_info,
      sharing_sync_preference_.GetTargetInfo(local_device_info->guid()),
      sharing_sync_preference_.GetEnabledFeatures(local_device_info));
  EXPECT_EQ(1, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());

  // Clearing SharingInfo should trigger RefreshLocalDeviceInfoCount.
  sharing_sync_preference_.ClearLocalSharingInfo();

  // Assume LocalDeviceInfoProvider has SharingInfo cleared.
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(base::nullopt);

  EXPECT_FALSE(sharing_sync_preference_.GetLocalSharingInfo());
  EXPECT_FALSE(
      sharing_sync_preference_.GetTargetInfo(local_device_info->guid()));
  EXPECT_TRUE(
      sharing_sync_preference_.GetEnabledFeatures(local_device_info).empty());
  EXPECT_EQ(2, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());
}

TEST_F(SharingSyncPreferenceTest,
       SyncAndRemoveLocalDevice_UseDeviceInfoDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kSharingUseDeviceInfo);

  auto sharing_info = GetDefaultSharingInfo();
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);

  // Sharing info is set but RefreshLocalDeviceInfoCount is not triggered.
  EXPECT_EQ(sharing_info, sharing_sync_preference_.GetLocalSharingInfo());
  EXPECT_EQ(0, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());

  // Assume LocalDeviceInfoProvider is updated now.
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(sharing_info);

  sharing_sync_preference_.ClearLocalSharingInfo();

  // Assume LocalDeviceInfoProvider has SharingInfo cleared.
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(base::nullopt);

  // Sharing info is cleared but RefreshLocalDeviceInfoCount is not triggered.
  EXPECT_FALSE(sharing_sync_preference_.GetLocalSharingInfo());
  EXPECT_EQ(0, fake_device_info_sync_service_.RefreshLocalDeviceInfoCount());
}

TEST_F(SharingSyncPreferenceTest, GetLocalSharingInfoFromProvider) {
  EXPECT_FALSE(sharing_sync_preference_.GetLocalSharingInfo());

  auto sharing_info = GetDefaultSharingInfo();
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(sharing_info);

  EXPECT_EQ(sharing_info, sharing_sync_preference_.GetLocalSharingInfo());
}

TEST_F(SharingSyncPreferenceTest, GetTargetInfoFromProvider) {
  std::unique_ptr<syncer::DeviceInfo> fake_device_info =
      std::make_unique<syncer::DeviceInfo>(
          kDeviceGuid, kDeviceName, "chrome_version", "user_agent",
          sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
          base::SysInfo::HardwareInfo(),
          /*last_updated_timestamp=*/base::Time::Now(),
          /*send_tab_to_self_receiving_enabled=*/false,
          /*sharing_info=*/base::nullopt);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info.get());
  EXPECT_FALSE(sharing_sync_preference_.GetTargetInfo(kDeviceGuid));

  auto sharing_info = GetDefaultSharingInfo();
  fake_device_info->set_sharing_info(sharing_info);

  ExpectSharingInfoEquals(
      sharing_info, sharing_sync_preference_.GetTargetInfo(kDeviceGuid),
      sharing_sync_preference_.GetEnabledFeatures(fake_device_info.get()));
}

TEST_F(SharingSyncPreferenceTest, FCMRegistrationGetSet) {
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());

  base::Time time_now = base::Time::Now();
  sharing_sync_preference_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(kAuthorizedEntity, time_now));

  auto fcm_registration = sharing_sync_preference_.GetFCMRegistration();
  EXPECT_TRUE(fcm_registration);
  EXPECT_EQ(kAuthorizedEntity, fcm_registration->authorized_entity);
  EXPECT_EQ(time_now, fcm_registration->timestamp);

  sharing_sync_preference_.ClearFCMRegistration();
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());
}

TEST_F(SharingSyncPreferenceTest, GetLocalSharingInfoForSync) {
  scoped_feature_list_.InitAndEnableFeature(kSharingUseDeviceInfo);

  auto sharing_info = GetDefaultSharingInfo();
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);

  EXPECT_EQ(sharing_info,
            SharingSyncPreference::GetLocalSharingInfoForSync(&prefs_));
}

TEST_F(SharingSyncPreferenceTest,
       GetLocalSharingInfoForSync_UseDeviceInfoDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kSharingUseDeviceInfo);

  auto sharing_info = GetDefaultSharingInfo();
  sharing_sync_preference_.SetLocalSharingInfo(sharing_info);

  EXPECT_FALSE(SharingSyncPreference::GetLocalSharingInfoForSync(&prefs_));
}

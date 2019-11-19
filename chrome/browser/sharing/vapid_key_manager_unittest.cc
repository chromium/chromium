// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/vapid_key_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class VapidKeyManagerTest : public testing::Test {
 protected:
  VapidKeyManagerTest()
      : sharing_sync_preference_(&prefs_, &fake_device_info_sync_service_),
        vapid_key_manager_(&sharing_sync_preference_, &test_sync_service_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  SharingSyncPreference sharing_sync_preference_;
  syncer::TestSyncService test_sync_service_;
  VapidKeyManager vapid_key_manager_;
};

}  // namespace

TEST_F(VapidKeyManagerTest, CreateKeyFlow) {
  scoped_feature_list_.InitAndDisableFeature(kSharingDeriveVapidKey);

  // No keys stored in preferences.
  EXPECT_EQ(base::nullopt, sharing_sync_preference_.GetVapidKey());

  // Expected to create new keys and store in preferences.
  crypto::ECPrivateKey* key_1 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_1);
  std::vector<uint8_t> key_info;
  EXPECT_TRUE(key_1->ExportPrivateKey(&key_info));
  EXPECT_EQ(key_info, sharing_sync_preference_.GetVapidKey());

  // Expected to return same key when called again.
  crypto::ECPrivateKey* key_2 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_2);
  std::vector<uint8_t> key_info_2;
  EXPECT_TRUE(key_2->ExportPrivateKey(&key_info_2));
  EXPECT_EQ(key_info, key_info_2);
}

TEST_F(VapidKeyManagerTest, ReadFromPreferenceFlow) {
  scoped_feature_list_.InitAndDisableFeature(kSharingDeriveVapidKey);

  // VAPID key already stored in preferences.
  auto preference_key_1 = crypto::ECPrivateKey::Create();
  ASSERT_TRUE(preference_key_1);
  std::vector<uint8_t> preference_key_info_1;
  ASSERT_TRUE(preference_key_1->ExportPrivateKey(&preference_key_info_1));
  sharing_sync_preference_.SetVapidKey(preference_key_info_1);

  // Expected to return key stored in preferences.
  crypto::ECPrivateKey* key_1 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_1);
  std::vector<uint8_t> key_info_1;
  EXPECT_TRUE(key_1->ExportPrivateKey(&key_info_1));
  EXPECT_EQ(preference_key_info_1, key_info_1);

  // Change VAPID key in sync prefernece.
  auto preference_key_2 = crypto::ECPrivateKey::Create();
  ASSERT_TRUE(preference_key_2);
  std::vector<uint8_t> preference_key_info_2;
  ASSERT_TRUE(preference_key_2->ExportPrivateKey(&preference_key_info_2));
  sharing_sync_preference_.SetVapidKey(preference_key_info_2);

  // Refresh local cache with new key in sync preference.
  EXPECT_TRUE(vapid_key_manager_.RefreshCachedKey());
  crypto::ECPrivateKey* key_2 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_2);
  std::vector<uint8_t> key_info_2;
  EXPECT_TRUE(key_2->ExportPrivateKey(&key_info_2));
  EXPECT_EQ(preference_key_info_2, key_info_2);
}

TEST_F(VapidKeyManagerTest, DeriveKeyFlow) {
  scoped_feature_list_.InitAndEnableFeature(kSharingDeriveVapidKey);
  test_sync_service_.SetExperimentalAuthenticationKey(
      crypto::ECPrivateKey::Create());

  // No keys stored in preferences.
  EXPECT_EQ(base::nullopt, sharing_sync_preference_.GetVapidKey());

  // Expected to derive key from sync secret and store in sync preferences.
  crypto::ECPrivateKey* key_1 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_1);
  std::vector<uint8_t> key_info_1;
  EXPECT_TRUE(key_1->ExportPrivateKey(&key_info_1));
  EXPECT_EQ(key_info_1, sharing_sync_preference_.GetVapidKey());

  // Change sync secret.
  test_sync_service_.SetExperimentalAuthenticationKey(
      crypto::ECPrivateKey::Create());

  // Refresh local cache with new sync secret.
  EXPECT_TRUE(vapid_key_manager_.RefreshCachedKey());
  crypto::ECPrivateKey* key_2 = vapid_key_manager_.GetOrCreateKey();
  EXPECT_TRUE(key_2);
  std::vector<uint8_t> key_info_2;
  EXPECT_TRUE(key_2->ExportPrivateKey(&key_info_2));
  EXPECT_NE(key_info_1, key_info_2);
}

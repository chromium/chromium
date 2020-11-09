// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_device_registration.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/common/pref_names.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service_factory.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/ec_private_key.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kAppID[] = "test_app_id";
const char kVapidFCMToken[] = "test_fcm_token";
const char kVapidFCMToken2[] = "test_fcm_token_2";
const char kSenderIdFCMToken[] = "sharing_fcm_token";
const char kDevicep256dh[] = "test_p256_dh";
const char kDevicep256dh2[] = "test_p256_dh_2";
const char kSenderIdP256dh[] = "sharing_p256dh";
const char kDeviceAuthSecret[] = "test_auth_secret";
const char kDeviceAuthSecret2[] = "test_auth_secret_2";
const char kSenderIdAuthSecret[] = "sharing_auth_secret";

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID,
               instance_id::InstanceID*(const std::string& app_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class FakeInstanceID : public instance_id::InstanceID {
 public:
  FakeInstanceID() : InstanceID(kAppID, /*gcm_driver = */ nullptr) {}
  ~FakeInstanceID() override = default;

  void GetID(GetIDCallback callback) override { NOTIMPLEMENTED(); }

  void GetCreationTime(GetCreationTimeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                base::TimeDelta time_to_live,
                const std::map<std::string, std::string>& options,
                std::set<Flags> flags,
                GetTokenCallback callback) override {
    if (authorized_entity == kSharingSenderID)
      std::move(callback).Run(kSenderIdFCMToken, result_);
    else
      std::move(callback).Run(fcm_token_, result_);
  }

  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteToken(const std::string& authorized_entity,
                   const std::string& scope,
                   DeleteTokenCallback callback) override {
    std::move(callback).Run(result_);
  }

  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteIDImpl(DeleteIDCallback callback) override { NOTIMPLEMENTED(); }

  void SetFCMResult(InstanceID::Result result) { result_ = result; }

  void SetFCMToken(std::string fcm_token) { fcm_token_ = std::move(fcm_token); }

  void GetEncryptionInfo(const std::string& authorized_entity,
                         GetEncryptionInfoCallback callback) override {
    if (authorized_entity == kSharingSenderID)
      std::move(callback).Run(kSenderIdP256dh, kSenderIdAuthSecret);
    else
      std::move(callback).Run(p256dh_, auth_secret_);
  }

  void SetEncryptionInfo(const std::string& p256dh,
                         const std::string& auth_secret) {
    p256dh_ = p256dh;
    auth_secret_ = auth_secret;
  }

 private:
  InstanceID::Result result_;
  std::string fcm_token_;
  std::string p256dh_ = kDevicep256dh;
  std::string auth_secret_ = kDeviceAuthSecret;
};

class SharingDeviceRegistrationTest : public testing::Test {
 public:
  SharingDeviceRegistrationTest()
      : sync_prefs_(&prefs_, &fake_device_info_sync_service_),
        vapid_key_manager_(&sync_prefs_, &test_sync_service_),
        sharing_device_registration_(pref_service_.get(),
                                     &sync_prefs_,
                                     &vapid_key_manager_,
                                     &mock_instance_id_driver_,
                                     &test_sync_service_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  static std::unique_ptr<PrefService> CreatePrefServiceAndRegisterPrefs() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable());
    registry->RegisterBooleanPref(prefs::kSharedClipboardEnabled, true);
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
    return factory.Create(registry);
  }

  void SetUp() {
    ON_CALL(mock_instance_id_driver_, GetInstanceID(testing::_))
        .WillByDefault(testing::Return(&fake_instance_id_));
  }

  void SetSharedClipboardPolicy(bool val) {
    pref_service_->SetBoolean(prefs::kSharedClipboardEnabled, val);
  }

  void RegisterDeviceSync() {
    base::RunLoop run_loop;
    sharing_device_registration_.RegisterDevice(
        base::BindLambdaForTesting([&](SharingDeviceRegistrationResult r) {
          result_ = r;
          local_sharing_info_ =
              SharingSyncPreference::GetLocalSharingInfoForSync(&prefs_);
          fcm_registration_ = sync_prefs_.GetFCMRegistration();
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void UnregisterDeviceSync() {
    base::RunLoop run_loop;
    sharing_device_registration_.UnregisterDevice(
        base::BindLambdaForTesting([&](SharingDeviceRegistrationResult r) {
          result_ = r;
          local_sharing_info_ =
              SharingSyncPreference::GetLocalSharingInfoForSync(&prefs_);
          fcm_registration_ = sync_prefs_.GetFCMRegistration();
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void SetInstanceIDFCMResult(instance_id::InstanceID::Result result) {
    fake_instance_id_.SetFCMResult(result);
  }

  void SetInstanceIDFCMToken(std::string fcm_token) {
    fake_instance_id_.SetFCMToken(std::move(fcm_token));
  }

  std::set<sync_pb::SharingSpecificFields::EnabledFeatures>
  GetExpectedEnabledFeatures(bool supports_vapid) {
    std::set<sync_pb::SharingSpecificFields::EnabledFeatures> features;

    // IsClickToCallSupported() involves JNI call which is hard to test.
    if (sharing_device_registration_.IsClickToCallSupported()) {
      features.insert(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
      if (supports_vapid)
        features.insert(sync_pb::SharingSpecificFields::CLICK_TO_CALL_VAPID);
    }

    // Shared clipboard should always be supported.
    features.insert(sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
    if (supports_vapid)
      features.insert(sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_VAPID);

    if (sharing_device_registration_.IsRemoteCopySupported())
      features.insert(sync_pb::SharingSpecificFields::REMOTE_COPY);

    return features;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  sync_preferences::TestingPrefServiceSyncable prefs_;
  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;
  FakeInstanceID fake_instance_id_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PrefService> pref_service_ =
      CreatePrefServiceAndRegisterPrefs();
  SharingSyncPreference sync_prefs_;
  syncer::TestSyncService test_sync_service_;
  VapidKeyManager vapid_key_manager_;
  SharingDeviceRegistration sharing_device_registration_;

  // callback results
  base::Optional<syncer::DeviceInfo::SharingInfo> local_sharing_info_;
  base::Optional<SharingSyncPreference::FCMRegistration> fcm_registration_;
  SharingDeviceRegistrationResult result_;
};

}  // namespace

TEST_F(SharingDeviceRegistrationTest, IsSharedClipboardSupported_True) {
  SetSharedClipboardPolicy(true);

  EXPECT_TRUE(sharing_device_registration_.IsSharedClipboardSupported());
}

TEST_F(SharingDeviceRegistrationTest, IsSharedClipboardSupported_False) {
  SetSharedClipboardPolicy(false);

  EXPECT_FALSE(sharing_device_registration_.IsSharedClipboardSupported());
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_Success) {
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES, syncer::SHARING_MESSAGE});
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::SUCCESS);
  SetInstanceIDFCMToken(kVapidFCMToken);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo());

  RegisterDeviceSync();

  std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features =
      GetExpectedEnabledFeatures(/*supports_vapid=*/true);
  syncer::DeviceInfo::SharingInfo expected_sharing_info(
      {kVapidFCMToken, kDevicep256dh, kDeviceAuthSecret},
      {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
      enabled_features);

  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_EQ(expected_sharing_info, local_sharing_info_);
  EXPECT_TRUE(fcm_registration_);

  SetInstanceIDFCMToken(kVapidFCMToken2);
  fake_instance_id_.SetEncryptionInfo(kDevicep256dh2, kDeviceAuthSecret2);
  RegisterDeviceSync();

  // Device should be re-registered with the new FCM token.
  syncer::DeviceInfo::SharingInfo expected_synced_sharing_info_2(
      {kVapidFCMToken2, kDevicep256dh2, kDeviceAuthSecret2},
      {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
      enabled_features);

  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_EQ(expected_synced_sharing_info_2, local_sharing_info_);
  EXPECT_TRUE(fcm_registration_);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_Vapid_Only) {
  scoped_feature_list_.InitAndDisableFeature(kSharingSendViaSync);
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::SUCCESS);
  SetInstanceIDFCMToken(kVapidFCMToken);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo());

  RegisterDeviceSync();

  std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features =
      GetExpectedEnabledFeatures(/*supports_vapid=*/true);
  syncer::DeviceInfo::SharingInfo expected_sharing_info(
      {kVapidFCMToken, kDevicep256dh, kDeviceAuthSecret},
      syncer::DeviceInfo::SharingTargetInfo(), enabled_features);

  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_EQ(expected_sharing_info, local_sharing_info_);
  EXPECT_TRUE(fcm_registration_);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_SenderIDOnly) {
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::SHARING_MESSAGE});
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::SUCCESS);
  SetInstanceIDFCMToken(kVapidFCMToken);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo());

  RegisterDeviceSync();

  std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features =
      GetExpectedEnabledFeatures(/*supports_vapid=*/false);
  syncer::DeviceInfo::SharingInfo expected_sharing_info(
      syncer::DeviceInfo::SharingTargetInfo(),
      {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
      enabled_features);

  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_EQ(expected_sharing_info, local_sharing_info_);
  EXPECT_TRUE(fcm_registration_);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_InternalError) {
  scoped_feature_list_.InitAndDisableFeature(kSharingSendViaSync);
  test_sync_service_.SetActiveDataTypes({});
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::SUCCESS);
  SetInstanceIDFCMToken(kVapidFCMToken);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo());

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistrationResult::kInternalError, result_);
  EXPECT_FALSE(local_sharing_info_);
  EXPECT_FALSE(fcm_registration_);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_NetworkError) {
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::NETWORK_ERROR);

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistrationResult::kFcmTransientError, result_);
  EXPECT_FALSE(local_sharing_info_);
  EXPECT_FALSE(fcm_registration_);
}

TEST_F(SharingDeviceRegistrationTest, RegisterDeviceTest_FatalError) {
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::DISABLED);

  RegisterDeviceSync();

  EXPECT_EQ(SharingDeviceRegistrationResult::kFcmFatalError, result_);
  EXPECT_FALSE(local_sharing_info_);
  EXPECT_FALSE(fcm_registration_);
}

TEST_F(SharingDeviceRegistrationTest, UnregisterDeviceTest_Success) {
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::SUCCESS);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo());

  // First register the device.
  RegisterDeviceSync();
  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_TRUE(local_sharing_info_);
  EXPECT_TRUE(fcm_registration_);

  // Then unregister the device.
  UnregisterDeviceSync();
  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_FALSE(local_sharing_info_);
  EXPECT_FALSE(fcm_registration_);

  // Further unregister does nothing and returns kDeviceNotRegistered.
  UnregisterDeviceSync();
  EXPECT_EQ(SharingDeviceRegistrationResult::kDeviceNotRegistered, result_);

  // Register the device again, Instance.GetToken will be attempted once more,
  // which will return a different FCM token.
  SetInstanceIDFCMToken(kVapidFCMToken2);
  RegisterDeviceSync();

  // Device should be registered with the new FCM token.
  std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features =
      GetExpectedEnabledFeatures(/*supports_vapid=*/true);
  syncer::DeviceInfo::SharingInfo expected_sharing_info(
      {kVapidFCMToken2, kDevicep256dh, kDeviceAuthSecret},
      {kSenderIdFCMToken, kSenderIdP256dh, kSenderIdAuthSecret},
      enabled_features);

  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_EQ(expected_sharing_info, local_sharing_info_);
  EXPECT_TRUE(fcm_registration_);
}

TEST_F(SharingDeviceRegistrationTest, UnregisterDeviceTest_SenderIDonly) {
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::SHARING_MESSAGE});
  SetInstanceIDFCMResult(instance_id::InstanceID::Result::SUCCESS);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo());

  // First register the device.
  RegisterDeviceSync();
  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_TRUE(local_sharing_info_);
  EXPECT_TRUE(fcm_registration_);
  EXPECT_FALSE(fcm_registration_->authorized_entity);

  // Then unregister the device.
  UnregisterDeviceSync();
  EXPECT_EQ(SharingDeviceRegistrationResult::kSuccess, result_);
  EXPECT_FALSE(local_sharing_info_);
  EXPECT_FALSE(fcm_registration_);
}

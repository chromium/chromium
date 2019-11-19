// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include <memory>

#include "base/base64.h"
#include "base/callback_list.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/sharing_utils.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kMessageId[] = "message_id";
const char kFcmToken[] = "fcm_token";
const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kAuthorizedEntity[] = "authorized_entity";
const int kTtlSeconds = 10;

class FakeGCMDriver : public gcm::FakeGCMDriver {
 public:
  FakeGCMDriver() {}
  ~FakeGCMDriver() override {}

  void SendWebPushMessage(const std::string& app_id,
                          const std::string& authorized_entity,
                          const std::string& p256dh,
                          const std::string& auth_secret,
                          const std::string& fcm_token,
                          crypto::ECPrivateKey* vapid_key,
                          gcm::WebPushMessage message,
                          gcm::WebPushCallback callback) override {
    app_id_ = app_id;
    authorized_entity_ = authorized_entity;
    p256dh_ = p256dh;
    auth_secret_ = auth_secret;
    fcm_token_ = fcm_token;
    vapid_key_ = vapid_key;
    message_ = std::move(message);
    std::move(callback).Run(result_,
                            base::make_optional<std::string>(kMessageId));
  }

  const std::string& app_id() { return app_id_; }
  const std::string& authorized_entity() { return authorized_entity_; }
  const std::string& p256dh() { return p256dh_; }
  const std::string& auth_secret() { return auth_secret_; }
  const std::string& fcm_token() { return fcm_token_; }
  crypto::ECPrivateKey* vapid_key() { return vapid_key_; }
  const gcm::WebPushMessage& message() { return message_; }

  void set_result(gcm::SendWebPushMessageResult result) { result_ = result; }

 private:
  std::string app_id_, authorized_entity_, p256dh_, auth_secret_, fcm_token_;
  crypto::ECPrivateKey* vapid_key_;
  gcm::WebPushMessage message_;
  gcm::SendWebPushMessageResult result_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMDriver);
};

class MockVapidKeyManager : public VapidKeyManager {
 public:
  MockVapidKeyManager()
      : VapidKeyManager(/*sharing_sync_preference=*/nullptr,
                        /*sync_service=*/nullptr) {}
  ~MockVapidKeyManager() {}

  MOCK_METHOD0(GetOrCreateKey, crypto::ECPrivateKey*());
};

class SharingFCMSenderTest : public testing::Test {
 public:
  void OnMessageSent(SharingSendMessageResult* result_out,
                     base::Optional<std::string>* message_id_out,
                     SharingSendMessageResult result,
                     base::Optional<std::string> message_id) {
    *result_out = result;
    *message_id_out = std::move(message_id);
  }

 protected:
  SharingFCMSenderTest()
      : sync_prefs_(&prefs_, &fake_device_info_sync_service),
        sharing_fcm_sender_(&fake_gcm_driver_,
                            &sync_prefs_,
                            &vapid_key_manager_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service;
  FakeGCMDriver fake_gcm_driver_;

  SharingSyncPreference sync_prefs_;
  testing::NiceMock<MockVapidKeyManager> vapid_key_manager_;
  SharingFCMSender sharing_fcm_sender_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};  // namespace

}  // namespace

TEST_F(SharingFCMSenderTest, NoFcmRegistration) {
  sync_prefs_.ClearFCMRegistration();

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(vapid_key.get()));

  syncer::DeviceInfo::SharingTargetInfo target{kFcmToken, kP256dh, kAuthSecret};

  SharingSendMessageResult result;
  base::Optional<std::string> message_id;
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_ack_message();
  sharing_fcm_sender_.SendMessageToDevice(
      std::move(target), base::TimeDelta::FromSeconds(kTtlSeconds),
      std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id));

  EXPECT_EQ(SharingSendMessageResult::kInternalError, result);
}

TEST_F(SharingFCMSenderTest, NoVapidKey) {
  sync_prefs_.SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));

  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(nullptr));

  syncer::DeviceInfo::SharingTargetInfo target{kFcmToken, kP256dh, kAuthSecret};

  SharingSendMessageResult result;
  base::Optional<std::string> message_id;
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_ack_message();
  sharing_fcm_sender_.SendMessageToDevice(
      std::move(target), base::TimeDelta::FromSeconds(kTtlSeconds),
      std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id));

  EXPECT_EQ(SharingSendMessageResult::kInternalError, result);
}

struct SharingFCMSenderResultTestData {
  const gcm::SendWebPushMessageResult web_push_result;
  const SharingSendMessageResult expected_result;
} kSharingFCMSenderResultTestData[] = {
    {gcm::SendWebPushMessageResult::kSuccessful,
     SharingSendMessageResult::kSuccessful},
    {gcm::SendWebPushMessageResult::kSuccessful,
     SharingSendMessageResult::kSuccessful},
    {gcm::SendWebPushMessageResult::kDeviceGone,
     SharingSendMessageResult::kDeviceNotFound},
    {gcm::SendWebPushMessageResult::kNetworkError,
     SharingSendMessageResult::kNetworkError},
    {gcm::SendWebPushMessageResult::kPayloadTooLarge,
     SharingSendMessageResult::kPayloadTooLarge},
    {gcm::SendWebPushMessageResult::kEncryptionFailed,
     SharingSendMessageResult::kInternalError},
    {gcm::SendWebPushMessageResult::kCreateJWTFailed,
     SharingSendMessageResult::kInternalError},
    {gcm::SendWebPushMessageResult::kServerError,
     SharingSendMessageResult::kInternalError},
    {gcm::SendWebPushMessageResult::kParseResponseFailed,
     SharingSendMessageResult::kInternalError},
    {gcm::SendWebPushMessageResult::kVapidKeyInvalid,
     SharingSendMessageResult::kInternalError}};

class SharingFCMSenderResultTest
    : public SharingFCMSenderTest,
      public testing::WithParamInterface<SharingFCMSenderResultTestData> {};

TEST_P(SharingFCMSenderResultTest, ResultTest) {
  sync_prefs_.SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));
  fake_gcm_driver_.set_result(GetParam().web_push_result);

  std::unique_ptr<crypto::ECPrivateKey> vapid_key =
      crypto::ECPrivateKey::Create();
  ON_CALL(vapid_key_manager_, GetOrCreateKey())
      .WillByDefault(testing::Return(vapid_key.get()));

  syncer::DeviceInfo::SharingTargetInfo target{kFcmToken, kP256dh, kAuthSecret};

  SharingSendMessageResult result;
  base::Optional<std::string> message_id;
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_ping_message();
  sharing_fcm_sender_.SendMessageToDevice(
      std::move(target), base::TimeDelta::FromSeconds(kTtlSeconds),
      std::move(sharing_message),
      base::BindOnce(&SharingFCMSenderTest::OnMessageSent,
                     base::Unretained(this), &result, &message_id));

  EXPECT_EQ(kSharingFCMAppID, fake_gcm_driver_.app_id());
  EXPECT_EQ(kAuthorizedEntity, fake_gcm_driver_.authorized_entity());
  EXPECT_EQ(kP256dh, fake_gcm_driver_.p256dh());
  EXPECT_EQ(kAuthSecret, fake_gcm_driver_.auth_secret());
  EXPECT_EQ(kFcmToken, fake_gcm_driver_.fcm_token());
  EXPECT_EQ(vapid_key.get(), fake_gcm_driver_.vapid_key());

  EXPECT_EQ(kTtlSeconds, fake_gcm_driver_.message().time_to_live);
  EXPECT_EQ(gcm::WebPushMessage::Urgency::kHigh,
            fake_gcm_driver_.message().urgency);
  chrome_browser_sharing::SharingMessage message_sent;
  message_sent.ParseFromString(fake_gcm_driver_.message().payload);
  EXPECT_TRUE(message_sent.has_ping_message());

  EXPECT_EQ(GetParam().expected_result, result);
  EXPECT_EQ(kMessageId, message_id);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SharingFCMSenderResultTest,
    testing::ValuesIn(kSharingFCMSenderResultTestData));

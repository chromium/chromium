// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_sender.h"

#include "base/bind_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/sharing_utils.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Test constants.
const char kReceiverGUID[] = "kReceiverGUID";
const char kReceiverDeviceName[] = "receiver_device";
const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kFCMToken[] = "vapid_fcm_token";
const char kAuthorizedEntity[] = "authorized_entity";
const char kSenderVapidFcmToken[] = "sender_vapid_fcm_token";
const char kSenderP256dh[] = "sender_p256dh";
const char kSenderAuthSecret[] = "sender_auth_secret";
const char kSenderMessageID[] = "sender_message_id";
constexpr base::TimeDelta kTimeToLive = base::TimeDelta::FromSeconds(10);

class MockSharingFCMSender : public SharingFCMSender {
 public:
  MockSharingFCMSender() : SharingFCMSender(nullptr, nullptr, nullptr) {}
  ~MockSharingFCMSender() override = default;

  MOCK_METHOD4(SendMessageToDevice,
               void(syncer::DeviceInfo::SharingTargetInfo target,
                    base::TimeDelta time_to_live,
                    SharingMessage message,
                    SendMessageCallback callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharingFCMSender);
};

class SharingMessageSenderTest : public testing::Test {
 public:
  SharingMessageSenderTest() {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }
  ~SharingMessageSenderTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service_;

  testing::NiceMock<MockSharingFCMSender>* mock_sharing_fcm_sender_ =
      new testing::NiceMock<MockSharingFCMSender>();
  SharingSyncPreference sharing_sync_preference_{
      &prefs_, &fake_device_info_sync_service_};

  SharingMessageSender sharing_message_sender_{
      base::WrapUnique(mock_sharing_fcm_sender_), &sharing_sync_preference_,
      fake_device_info_sync_service_.GetLocalDeviceInfoProvider()};

  DISALLOW_COPY_AND_ASSIGN(SharingMessageSenderTest);
};

static syncer::DeviceInfo::SharingInfo CreateLocalSharingInfo() {
  return syncer::DeviceInfo::SharingInfo(
      {kSenderVapidFcmToken, kSenderP256dh, kSenderAuthSecret},
      {"sender_id_fcm_token", "sender_id_p256dh", "sender_id_auth_secret"},
      std::set<sync_pb::SharingSpecificFields::EnabledFeatures>());
}

static std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& id,
    const std::string& name) {
  return std::make_unique<syncer::DeviceInfo>(
      id, name, "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
      base::SysInfo::HardwareInfo(),
      /*last_updated_timestamp=*/base::Time::Now(),
      /*send_tab_to_self_receiving_enabled=*/false,
      syncer::DeviceInfo::SharingInfo(
          {kFCMToken, kP256dh, kAuthSecret},
          {"sender_id_fcm_token", "sender_id_p256dh", "sender_id_auth_secret"},
          std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
              sync_pb::SharingSpecificFields::CLICK_TO_CALL}));
}

MATCHER_P(ProtoEquals, message, "") {
  if (!arg)
    return false;

  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg->SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

TEST_F(SharingMessageSenderTest, MessageSent_AckTimedout) {
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(kReceiverGUID, kReceiverDeviceName);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(CreateLocalSharingInfo());
  sharing_sync_preference_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(kAuthorizedEntity,
                                             base::Time::Now()));

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kAckTimeout),
                  testing::Eq(nullptr)));

  auto simulate_timeout = [&](syncer::DeviceInfo::SharingTargetInfo target,
                              base::TimeDelta time_to_live,
                              chrome_browser_sharing::SharingMessage message,
                              SharingFCMSender::SendMessageCallback callback) {
    // FCM message sent successfully.
    std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                            kSenderMessageID);
    task_environment_.FastForwardBy(kTimeToLive);

    // Callback already run with result timeout, ack received for same message
    // id is ignored.
    sharing_message_sender_.OnAckReceived(
        SharingPayloadCaseToMessageType(message.payload_case()),
        kSenderMessageID, /*response=*/nullptr);
  };

  ON_CALL(*mock_sharing_fcm_sender_,
          SendMessageToDevice(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(simulate_timeout));

  sharing_message_sender_.SendMessageToDevice(
      kReceiverGUID, kTimeToLive, chrome_browser_sharing::SharingMessage(),
      mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, SendMessageToDevice_InternalError) {
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(kReceiverGUID, kReceiverDeviceName);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(CreateLocalSharingInfo());
  sharing_sync_preference_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(kAuthorizedEntity,
                                             base::Time::Now()));

  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kInternalError),
                  testing::Eq(nullptr)));

  auto simulate_internal_error =
      [&](syncer::DeviceInfo::SharingTargetInfo target,
          base::TimeDelta time_to_live,
          chrome_browser_sharing::SharingMessage message,
          SharingFCMSender::SendMessageCallback callback) {
        // FCM message not sent succesfully.
        std::move(callback).Run(SharingSendMessageResult::kInternalError,
                                base::nullopt);

        // Callback already run with result timeout, ack received for same
        // message id is ignored.
        sharing_message_sender_.OnAckReceived(
            SharingPayloadCaseToMessageType(message.payload_case()),
            kSenderMessageID, /*response=*/nullptr);
      };

  ON_CALL(*mock_sharing_fcm_sender_,
          SendMessageToDevice(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(simulate_internal_error));

  sharing_message_sender_.SendMessageToDevice(
      kReceiverGUID, kTimeToLive, chrome_browser_sharing::SharingMessage(),
      mock_callback.Get());
}

TEST_F(SharingMessageSenderTest, MessageSent_AckReceived) {
  std::unique_ptr<syncer::DeviceInfo> device_info =
      CreateFakeDeviceInfo(kReceiverGUID, kReceiverDeviceName);
  fake_device_info_sync_service_.GetDeviceInfoTracker()->Add(device_info.get());
  fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
      ->GetMutableDeviceInfo()
      ->set_sharing_info(CreateLocalSharingInfo());
  sharing_sync_preference_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(kAuthorizedEntity,
                                             base::Time::Now()));

  chrome_browser_sharing::SharingMessage sent_message;
  sent_message.mutable_click_to_call_message()->set_phone_number("999999");

  chrome_browser_sharing::ResponseMessage expected_response_message;
  base::MockCallback<SharingMessageSender::ResponseCallback> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(testing::Eq(SharingSendMessageResult::kSuccessful),
                  ProtoEquals(expected_response_message)));

  auto simulate_expected_ack_message_received =
      [&](syncer::DeviceInfo::SharingTargetInfo target,
          base::TimeDelta time_to_live,
          chrome_browser_sharing::SharingMessage message,
          SharingFCMSender::SendMessageCallback callback) {
        // FCM message sent successfully.
        std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                                kSenderMessageID);

        // Check sender info details.
        const syncer::DeviceInfo* local_device =
            fake_device_info_sync_service_.GetLocalDeviceInfoProvider()
                ->GetLocalDeviceInfo();
        ASSERT_EQ(local_device->guid(), message.sender_guid());
        ASSERT_EQ(GetSharingDeviceNames(local_device).full_name,
                  message.sender_device_name());
        ASSERT_TRUE(local_device->sharing_info().has_value());
        ASSERT_EQ(kSenderVapidFcmToken, message.sender_info().fcm_token());
        ASSERT_EQ(kSenderP256dh, message.sender_info().p256dh());
        ASSERT_EQ(kSenderAuthSecret, message.sender_info().auth_secret());

        // Simulate ack message received.
        std::unique_ptr<chrome_browser_sharing::ResponseMessage>
            response_message =
                std::make_unique<chrome_browser_sharing::ResponseMessage>();
        response_message->CopyFrom(expected_response_message);

        sharing_message_sender_.OnAckReceived(
            SharingPayloadCaseToMessageType(message.payload_case()),
            kSenderMessageID, std::move(response_message));
      };

  ON_CALL(*mock_sharing_fcm_sender_,
          SendMessageToDevice(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(simulate_expected_ack_message_received));

  sharing_message_sender_.SendMessageToDevice(
      kReceiverGUID, kTimeToLive, std::move(sent_message), mock_callback.Get());
}

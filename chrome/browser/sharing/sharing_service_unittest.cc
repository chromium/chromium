// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_service.h"

#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/sharing/fake_device_info.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/mock_sharing_device_source.h"
#include "chrome/browser/sharing/mock_sharing_message_sender.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_handler_registry.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kP256dh[] = "p256dh";
const char kAuthSecret[] = "auth_secret";
const char kVapidFcmToken[] = "vapid_fcm_token";
const char kSharingFcmToken[] = "sharing_fcm_token";
const char kDeviceName[] = "other_name";
const char kAuthorizedEntity[] = "authorized_entity";
constexpr base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(15);

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class MockSharingHandlerRegistry : public SharingHandlerRegistry {
 public:
  MockSharingHandlerRegistry() = default;
  ~MockSharingHandlerRegistry() override = default;

  MOCK_METHOD1(
      GetSharingHandler,
      SharingMessageHandler*(
          chrome_browser_sharing::SharingMessage::PayloadCase payload_case));
  MOCK_METHOD2(
      RegisterSharingHandler,
      void(std::unique_ptr<SharingMessageHandler> handler,
           chrome_browser_sharing::SharingMessage::PayloadCase payload_case));
  MOCK_METHOD1(
      UnregisterSharingHandler,
      void(chrome_browser_sharing::SharingMessage::PayloadCase payload_case));
};

class MockSharingFCMHandler : public SharingFCMHandler {
  using SharingMessage = chrome_browser_sharing::SharingMessage;

 public:
  MockSharingFCMHandler()
      : SharingFCMHandler(/*gcm_driver=*/nullptr,
                          /*sharing_fcm_sender=*/nullptr,
                          /*sync_preference=*/nullptr,
                          /*handler_registry=*/nullptr) {}
  ~MockSharingFCMHandler() override = default;

  MOCK_METHOD0(StartListening, void());
  MOCK_METHOD0(StopListening, void());
};

class FakeSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  FakeSharingDeviceRegistration(
      PrefService* pref_service,
      SharingSyncPreference* prefs,
      VapidKeyManager* vapid_key_manager,
      instance_id::InstanceIDDriver* instance_id_driver,
      syncer::SyncService* sync_service)
      : SharingDeviceRegistration(pref_service,
                                  prefs,
                                  vapid_key_manager,
                                  instance_id_driver,
                                  sync_service),
        vapid_key_manager_(vapid_key_manager) {}
  ~FakeSharingDeviceRegistration() override = default;

  void RegisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {
    registration_attempts_++;
    // Simulate SharingDeviceRegistration calling GetOrCreateKey.
    vapid_key_manager_->GetOrCreateKey();
    std::move(callback).Run(result_);
  }

  void UnregisterDevice(
      SharingDeviceRegistration::RegistrationCallback callback) override {
    unregistration_attempts_++;
    std::move(callback).Run(result_);
  }

  void SetResult(SharingDeviceRegistrationResult result) { result_ = result; }

  int registration_attempts() { return registration_attempts_; }
  int unregistration_attempts() { return unregistration_attempts_; }

 private:
  VapidKeyManager* vapid_key_manager_;
  SharingDeviceRegistrationResult result_ =
      SharingDeviceRegistrationResult::kSuccess;
  int registration_attempts_ = 0;
  int unregistration_attempts_ = 0;
};

class SharingServiceTest : public testing::Test {
 public:
  SharingServiceTest() {
    sync_prefs_ =
        new SharingSyncPreference(&prefs_, &fake_device_info_sync_service);
    vapid_key_manager_ = new VapidKeyManager(sync_prefs_, &test_sync_service_);
    sharing_device_registration_ = new FakeSharingDeviceRegistration(
        /* pref_service= */ nullptr, sync_prefs_, vapid_key_manager_,
        &mock_instance_id_driver_, &test_sync_service_);
    handler_registry_ = new testing::NiceMock<MockSharingHandlerRegistry>();
    fcm_handler_ = new testing::NiceMock<MockSharingFCMHandler>();
    device_source_ = new testing::NiceMock<MockSharingDeviceSource>();
    sharing_message_sender_ = new testing::NiceMock<MockSharingMessageSender>();
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());

    ON_CALL(*device_source_, IsReady()).WillByDefault(testing::Return(true));
  }

  ~SharingServiceTest() override {
    // Make sure we're creating a SharingService so it can take ownership of the
    // local objects.
    GetSharingService();
  }

  void OnMessageSent(
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
    send_message_result_ = base::make_optional(result);
    send_message_response_ = std::move(response);
  }

  const base::Optional<SharingSendMessageResult>& send_message_result() {
    return send_message_result_;
  }

  const chrome_browser_sharing::ResponseMessage* send_message_response() {
    return send_message_response_.get();
  }

  void OnDeviceCandidatesInitialized() {
    device_candidates_initialized_ = true;
  }

 protected:
  static syncer::DeviceInfo::SharingInfo CreateSharingInfo() {
    return syncer::DeviceInfo::SharingInfo(
        {kVapidFcmToken, kP256dh, kAuthSecret},
        {kSharingFcmToken, kP256dh, kAuthSecret},
        std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
            sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2});
  }

  // Lazily initialized so we can test the constructor.
  SharingService* GetSharingService() {
    if (!sharing_service_) {
      sharing_service_ = std::make_unique<SharingService>(
          base::WrapUnique(sync_prefs_), base::WrapUnique(vapid_key_manager_),
          base::WrapUnique(sharing_device_registration_),
          base::WrapUnique(sharing_message_sender_),
          base::WrapUnique(device_source_), base::WrapUnique(handler_registry_),
          base::WrapUnique(fcm_handler_), &test_sync_service_);
    }
    task_environment_.RunUntilIdle();
    return sharing_service_.get();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  syncer::FakeDeviceInfoSyncService fake_device_info_sync_service;
  syncer::TestSyncService test_sync_service_;
  sync_preferences::TestingPrefServiceSyncable prefs_;

  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  testing::NiceMock<MockSharingHandlerRegistry>* handler_registry_;
  testing::NiceMock<MockSharingFCMHandler>* fcm_handler_;
  testing::NiceMock<MockSharingDeviceSource>* device_source_;

  SharingSyncPreference* sync_prefs_;
  VapidKeyManager* vapid_key_manager_;
  FakeSharingDeviceRegistration* sharing_device_registration_;
  testing::NiceMock<MockSharingMessageSender>* sharing_message_sender_;
  bool device_candidates_initialized_ = false;

 private:
  std::unique_ptr<SharingService> sharing_service_ = nullptr;
  base::Optional<SharingSendMessageResult> send_message_result_;
  std::unique_ptr<chrome_browser_sharing::ResponseMessage>
      send_message_response_;
};

bool ProtoEquals(const google::protobuf::MessageLite& expected,
                 const google::protobuf::MessageLite& actual) {
  std::string expected_serialized, actual_serialized;
  expected.SerializeToString(&expected_serialized);
  actual.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}
}  // namespace

TEST_F(SharingServiceTest, GetDeviceCandidates_Empty) {
  EXPECT_CALL(*device_source_, GetDeviceCandidates(::testing::_))
      .WillOnce(
          [](sync_pb::SharingSpecificFields::EnabledFeatures required_feature)
              -> std::vector<std::unique_ptr<syncer::DeviceInfo>> {
            return {};
          });

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);
  EXPECT_TRUE(candidates.empty());
}

TEST_F(SharingServiceTest, GetDeviceCandidates_Tracked) {
  EXPECT_CALL(*device_source_, GetDeviceCandidates(::testing::_))
      .WillOnce(
          [](sync_pb::SharingSpecificFields::EnabledFeatures required_feature)
              -> std::vector<std::unique_ptr<syncer::DeviceInfo>> {
            std::vector<std::unique_ptr<syncer::DeviceInfo>> device_candidates;
            device_candidates.push_back(CreateFakeDeviceInfo(
                base::GenerateGUID(), kDeviceName, CreateSharingInfo()));
            return device_candidates;
          });

  std::vector<std::unique_ptr<syncer::DeviceInfo>> candidates =
      GetSharingService()->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2);

  ASSERT_EQ(1u, candidates.size());
}

TEST_F(SharingServiceTest, SendMessageToDeviceSuccess) {
  std::unique_ptr<syncer::DeviceInfo> device_info = CreateFakeDeviceInfo(
      base::GenerateGUID(), kDeviceName, CreateSharingInfo());

  chrome_browser_sharing::ResponseMessage expected_response_message;

  auto run_callback = [&](const syncer::DeviceInfo& device_info,
                          base::TimeDelta response_timeout,
                          chrome_browser_sharing::SharingMessage message,
                          SharingMessageSender::DelegateType delegate_type,
                          SharingMessageSender::ResponseCallback callback) {
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response_message =
        std::make_unique<chrome_browser_sharing::ResponseMessage>();
    response_message->CopyFrom(expected_response_message);
    std::move(callback).Run(SharingSendMessageResult::kSuccessful,
                            std::move(response_message));
  };

  ON_CALL(*sharing_message_sender_,
          SendMessageToDevice(testing::_, testing::_, testing::_, testing::_,
                              testing::_))
      .WillByDefault(testing::Invoke(run_callback));

  GetSharingService()->SendMessageToDevice(
      *device_info.get(), kTimeout, chrome_browser_sharing::SharingMessage(),
      base::BindOnce(&SharingServiceTest::OnMessageSent,
                     base::Unretained(this)));

  EXPECT_EQ(SharingSendMessageResult::kSuccessful, send_message_result());
  ASSERT_TRUE(send_message_response());
  EXPECT_TRUE(ProtoEquals(expected_response_message, *send_message_response()));
}

TEST_F(SharingServiceTest, DeviceRegistration) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // As device is already registered, won't attempt registration anymore.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  auto vapid_key = crypto::ECPrivateKey::Create();
  ASSERT_TRUE(vapid_key);
  std::vector<uint8_t> vapid_key_info;
  ASSERT_TRUE(vapid_key->ExportPrivateKey(&vapid_key_info));

  // Registration will be attempeted as VAPID key has changed.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  sync_prefs_->SetVapidKey(vapid_key_info);
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationPreferenceNotAvailable) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(syncer::DEVICE_INFO);

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // As sync preferences is not available, registration shouldn't start.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(0, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationTransportMode) {
  // Enable the transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kSharingSendViaSync},
      /*disabled_features=*/{});

  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::SHARING_MESSAGE});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegistrationTransientError) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Retry will be scheduled on transient error received.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kFcmTransientError);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::REGISTERING,
            GetSharingService()->GetStateForTesting());

  // Retry should be scheduled by now. Next retry after 5 minutes will be
  // successful.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kRetryBackoffPolicy.initial_delay_ms));
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceUnregistrationSyncDisabled) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);

  // Create new SharingService instance with sync disabled at constructor.
  GetSharingService();
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, DeviceRegisterAndUnregister) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  // Create new SharingService instance with feature enabled at constructor.
  GetSharingService();
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Expect registration to be successful on sync state changed.
  sharing_device_registration_->SetResult(
      SharingDeviceRegistrationResult::kSuccess);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Change sync to configuring, which will be ignored.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(0, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Disable sync and un-registration should happen.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Further state changes do nothing.
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(0);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(1, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());

  // Should be able to register once again when sync is back on.
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(1, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::ACTIVE,
            GetSharingService()->GetStateForTesting());

  // Disable syncing of preference and un-registration should happen.
  test_sync_service_.SetActiveDataTypes(syncer::DEVICE_INFO);
  EXPECT_CALL(*fcm_handler_, StopListening()).Times(1);
  test_sync_service_.FireStateChanged();
  EXPECT_EQ(2, sharing_device_registration_->registration_attempts());
  EXPECT_EQ(2, sharing_device_registration_->unregistration_attempts());
  EXPECT_EQ(SharingService::State::DISABLED,
            GetSharingService()->GetStateForTesting());
}

TEST_F(SharingServiceTest, StartListeningToFCMAtConstructor) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  // Create new SharingService instance with FCM already registered at
  // constructor.
  sync_prefs_->SetFCMRegistration(SharingSyncPreference::FCMRegistration(
      kAuthorizedEntity, base::Time::Now()));
  EXPECT_CALL(*fcm_handler_, StartListening()).Times(1);
  GetSharingService();
}

TEST_F(SharingServiceTest, GetDeviceByGuid) {
  std::string guid = base::GenerateGUID();
  EXPECT_CALL(*device_source_, GetDeviceByGuid(guid))
      .WillOnce(
          [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
            return CreateFakeDeviceInfo(guid, "Dell Computer sno one");
          });

  std::unique_ptr<syncer::DeviceInfo> device_info =
      GetSharingService()->GetDeviceByGuid(guid);
  EXPECT_EQ("Dell Computer sno one", device_info->client_name());
}

TEST_F(SharingServiceTest, AddSharingHandler) {
  EXPECT_CALL(*handler_registry_,
              RegisterSharingHandler(testing::_, testing::_))
      .Times(1);
  GetSharingService()->RegisterSharingHandler(
      nullptr, chrome_browser_sharing::SharingMessage::kSharedClipboardMessage);
}

TEST_F(SharingServiceTest, RemoveSharingHandler) {
  EXPECT_CALL(*handler_registry_, UnregisterSharingHandler(testing::_))
      .Times(1);
  GetSharingService()->UnregisterSharingHandler(
      chrome_browser_sharing::SharingMessage::kSharedClipboardMessage);
}

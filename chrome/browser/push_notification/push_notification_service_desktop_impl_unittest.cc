// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/push_notification/prefs/push_notification_prefs.h"
#include "chrome/browser/push_notification/server_client/fake_push_notification_server_client.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client_desktop_impl.h"
#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler_factory.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/push_notification/fake_push_notification_client.h"
#include "components/push_notification/push_notification_constants.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPushNotificationAppId[] = "com.google.chrome.push_notification";
const char kSenderIdFCMToken[] = "sharing_fcm_token";
const char kSharingSenderID[] = "745476177629";
const char kTestMessage[] = "This is a test message";
const char kTestRepresentativeTargetId[] = "0123456789";
const char kTotalTokenRetrievalTime[] =
    "PushNotification.ChromeOS.GCM.Token.RetrievalTime";
const char kTotalSuccessfulRegistrationResponseTime[] =
    "PushNotification.ChromeOS.MultiLoginUpdateApi.ResponseTime.Success";
const char kTotalFailedRegistrationResponseTime[] =
    "PushNotification.ChromeOS.MultiLoginUpdateApi.ResponseTime.Failure";
const char kGcmTokenRetrievalResult[] =
    "PushNotification.ChromeOS.GCM.Token.RetrievalResult";
const char kServiceRegistrationResult[] =
    "PushNotification.ChromeOS.Registration.Result";

class FakeInstanceID : public instance_id::InstanceID {
 public:
  explicit FakeInstanceID(gcm::FakeGCMDriver* gcm_driver)
      : InstanceID(kPushNotificationAppId, gcm_driver) {}
  ~FakeInstanceID() override = default;

  void GetID(GetIDCallback callback) override { NOTIMPLEMENTED(); }

  void GetCreationTime(GetCreationTimeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                base::TimeDelta time_to_live,
                std::set<Flags> flags,
                GetTokenCallback callback) override {
    if (authorized_entity == kSharingSenderID) {
      std::move(callback).Run(kSenderIdFCMToken, result_);
    } else {
      std::move(callback).Run(fcm_token_, result_);
    }
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

 private:
  InstanceID::Result result_;
  std::string fcm_token_;
};

class FakeInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  FakeInstanceIDDriver() : InstanceIDDriver(nullptr) {}

  FakeInstanceIDDriver(const FakeInstanceIDDriver&) = delete;
  FakeInstanceIDDriver& operator=(const FakeInstanceIDDriver&) = delete;

  ~FakeInstanceIDDriver() override = default;

  instance_id::InstanceID* GetInstanceID(const std::string& app_id) override {
    return fake_instance_id_;
  }

  void SetFakeInstanceID(raw_ptr<FakeInstanceID> fake_instance_id) {
    fake_instance_id_ = std::move(fake_instance_id);
  }

 private:
  raw_ptr<FakeInstanceID> fake_instance_id_;
};

}  // namespace

namespace push_notification {

class PushNotificationServiceDesktopImplTest : public testing::Test {
 public:
  PushNotificationServiceDesktopImplTest() {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  ~PushNotificationServiceDesktopImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    RegisterPushNotificationPrefs(pref_service_.registry());
    ash::nearby::NearbySchedulerFactory::SetFactoryForTesting(
        &scheduler_factory_);
    PushNotificationServerClientDesktopImpl::Factory::SetFactoryForTesting(
        &fake_client_factory_);
    fake_gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();
    fake_instance_id_ =
        std::make_unique<FakeInstanceID>(fake_gcm_driver_.get());
    fake_instance_id_driver_.SetFakeInstanceID(fake_instance_id_.get());
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>(
        &test_url_loader_factory_, nullptr, nullptr);

    push_notification_service_ =
        std::make_unique<PushNotificationServiceDesktopImpl>(
            &pref_service_, &fake_instance_id_driver_,
            identity_test_env_->identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_));
    histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 0);
    histogram_tester_.ExpectTotalCount(kTotalSuccessfulRegistrationResponseTime,
                                       0);
    histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 0);
    histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                        /*bucket: failure=*/0, 0);
    histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                        /*bucket: success=*/1, 0);
    histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                        /*bucket: failure=*/0, 0);
    histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                        /*bucket: success=*/1, 0);
  }

  void TearDown() override {
    push_notification_service_.reset();
    identity_test_env_.reset();
    fake_gcm_driver_.reset();
  }

  push_notification::proto::NotificationsMultiLoginUpdateResponse
  CreateResponseProto() {
    push_notification::proto::NotificationsMultiLoginUpdateResponse
        response_proto;
    push_notification::proto::NotificationsMultiLoginUpdateResponse::
        RegistrationResult* registration_result =
            response_proto.add_registration_results();
    push_notification::proto::StatusProto* status =
        registration_result->mutable_status();
    status->set_code(0);
    status->set_message("OK");
    push_notification::proto::Target* target =
        registration_result->mutable_target();
    target->set_representative_target_id(kTestRepresentativeTargetId);
    return response_proto;
  }

  void CheckForSuccessfulRegistration() {
    EXPECT_TRUE(fake_client_factory_.fake_server_client()
                    ->HasRegisterWithPushNotificationServiceCallback());
    EXPECT_TRUE(fake_client_factory_.fake_server_client()->HasErrorCallback());
    fake_client_factory_.fake_server_client()
        ->InvokeRegisterWithPushNotificationServiceSuccessCallback(
            CreateResponseProto());
    EXPECT_TRUE(push_notification_service_->IsServiceInitialized());
    EXPECT_EQ(kTestRepresentativeTargetId,
              pref_service_.GetString(
                  prefs::kPushNotificationRepresentativeTargetIdPrefName));
    histogram_tester_.ExpectTotalCount(kTotalSuccessfulRegistrationResponseTime,
                                       1);
  }

  void CheckForFailedRegistration(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
          error) {
    EXPECT_TRUE(fake_client_factory_.fake_server_client()
                    ->HasRegisterWithPushNotificationServiceCallback());
    EXPECT_TRUE(fake_client_factory_.fake_server_client()->HasErrorCallback());
    fake_client_factory_.fake_server_client()
        ->InvokeRegisterWithPushNotificationServiceErrorCallback(error);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PushNotificationServiceDesktopImpl>
      push_notification_service_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<FakeInstanceID> fake_instance_id_;
  std::unique_ptr<gcm::FakeGCMDriver> fake_gcm_driver_;
  FakeInstanceIDDriver fake_instance_id_driver_;
  FakePushNotificationServerClient::Factory fake_client_factory_;
  ash::nearby::FakeNearbySchedulerFactory scheduler_factory_;
};

TEST_F(PushNotificationServiceDesktopImplTest, StartService) {
  fake_instance_id_->SetFCMResult(instance_id::InstanceID::Result::SUCCESS);
  fake_instance_id_->SetFCMToken(kSenderIdFCMToken);
  ash::nearby::FakeNearbyScheduler* registration_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(
              prefs::
                  kPushNotificationRegistrationAttemptBackoffSchedulerPrefName)
          ->second.fake_scheduler;
  registration_scheduler->InvokeRequestCallback();
  EXPECT_EQ(std::string(), fake_client_factory_.fake_server_client()
                               ->GetRequestProto()
                               .target()
                               .representative_target_id());
  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 1);
  CheckForSuccessfulRegistration();
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 0);
  histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServiceDesktopImplTest, StartServiceWithPref) {
  fake_instance_id_->SetFCMResult(instance_id::InstanceID::Result::SUCCESS);
  fake_instance_id_->SetFCMToken(kSenderIdFCMToken);
  pref_service_.SetString(
      prefs::kPushNotificationRepresentativeTargetIdPrefName,
      kTestRepresentativeTargetId);
  ash::nearby::FakeNearbyScheduler* registration_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(
              prefs::
                  kPushNotificationRegistrationAttemptBackoffSchedulerPrefName)
          ->second.fake_scheduler;
  registration_scheduler->InvokeRequestCallback();
  EXPECT_EQ(kTestRepresentativeTargetId,
            fake_client_factory_.fake_server_client()
                ->GetRequestProto()
                .target()
                .representative_target_id());
  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 1);
  CheckForSuccessfulRegistration();
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 0);
  histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServiceDesktopImplTest, StartServiceWithPrefStoreReset) {
  fake_instance_id_->SetFCMResult(instance_id::InstanceID::Result::SUCCESS);
  fake_instance_id_->SetFCMToken(kSenderIdFCMToken);
  pref_service_.SetString(
      prefs::kPushNotificationRepresentativeTargetIdPrefName,
      kTestRepresentativeTargetId);
  ash::nearby::FakeNearbyScheduler* registration_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(
              prefs::
                  kPushNotificationRegistrationAttemptBackoffSchedulerPrefName)
          ->second.fake_scheduler;
  registration_scheduler->InvokeRequestCallback();
  EXPECT_EQ(kTestRepresentativeTargetId,
            fake_client_factory_.fake_server_client()
                ->GetRequestProto()
                .target()
                .representative_target_id());
  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 1);
  CheckForSuccessfulRegistration();
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 0);
  histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: success=*/1, 1);
  push_notification_service_->OnStoreReset();
  EXPECT_EQ(std::string(),
            pref_service_.GetString(
                prefs::kPushNotificationRepresentativeTargetIdPrefName));
}

TEST_F(PushNotificationServiceDesktopImplTest, StartServiceTokenFailure) {
  fake_instance_id_->SetFCMResult(
      instance_id::InstanceID::Result::SERVER_ERROR);

  ash::nearby::FakeNearbyScheduler* registration_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(
              prefs::
                  kPushNotificationRegistrationAttemptBackoffSchedulerPrefName)
          ->second.fake_scheduler;
  registration_scheduler->InvokeRequestCallback();

  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 0);
  EXPECT_FALSE(fake_client_factory_.fake_server_client());
  EXPECT_FALSE(push_notification_service_->IsServiceInitialized());
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 0);
  histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                      /*bucket: failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: success=*/1, 0);
}

TEST_F(PushNotificationServiceDesktopImplTest,
       StartServiceAuthenticationFailure) {
  fake_instance_id_->SetFCMResult(instance_id::InstanceID::Result::SUCCESS);
  fake_instance_id_->SetFCMToken(kSenderIdFCMToken);

  ash::nearby::FakeNearbyScheduler* registration_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(
              prefs::
                  kPushNotificationRegistrationAttemptBackoffSchedulerPrefName)
          ->second.fake_scheduler;
  registration_scheduler->InvokeRequestCallback();

  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 1);
  CheckForFailedRegistration(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
          kAuthenticationError);
  histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 1);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: success=*/1, 0);
}

TEST_F(PushNotificationServiceDesktopImplTest,
       StartServiceAuthenticationFailureRetrySuccess) {
  fake_instance_id_->SetFCMResult(instance_id::InstanceID::Result::SUCCESS);
  fake_instance_id_->SetFCMToken(kSenderIdFCMToken);

  ash::nearby::FakeNearbyScheduler* registration_scheduler =
      scheduler_factory_.pref_name_to_on_demand_instance()
          .find(
              prefs::
                  kPushNotificationRegistrationAttemptBackoffSchedulerPrefName)
          ->second.fake_scheduler;
  registration_scheduler->InvokeRequestCallback();

  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 1);
  CheckForFailedRegistration(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
          kAuthenticationError);
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 1);

  registration_scheduler->InvokeRequestCallback();

  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 2);
  CheckForFailedRegistration(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError::
          kAuthenticationError);
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 2);

  registration_scheduler->InvokeRequestCallback();

  histogram_tester_.ExpectTotalCount(kTotalTokenRetrievalTime, 3);
  CheckForSuccessfulRegistration();
  histogram_tester_.ExpectTotalCount(kTotalFailedRegistrationResponseTime, 2);
  histogram_tester_.ExpectBucketCount(kGcmTokenRetrievalResult,
                                      /*bucket: success=*/1, 3);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: failure=*/0, 2);
  histogram_tester_.ExpectBucketCount(kServiceRegistrationResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServiceDesktopImplTest, OnMessageReceived) {
  // No need to invoke the scheduler here because we aren't performing any
  // actions that require initialization to be complete.

  auto* client_manager =
      push_notification_service_->GetPushNotificationClientManager();
  auto fake_push_notification_client =
      std::make_unique<FakePushNotificationClient>(
          push_notification::ClientId::kNearbyPresence);
  client_manager->AddPushNotificationClient(
      fake_push_notification_client.get());
  gcm::IncomingMessage message;
  message.data.insert_or_assign(kNotificationTypeIdKey,
                                push_notification::kNearbyPresenceClientId);
  message.data.insert_or_assign(kNotificationPayloadKey, kTestMessage);

  push_notification_service_->OnMessage(kPushNotificationAppId,
                                        std::move(message));
  EXPECT_EQ(
      kTestMessage,
      fake_push_notification_client->GetMostRecentMessageDataReceived().at(
          kNotificationPayloadKey));
}

}  // namespace push_notification

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

std::unique_ptr<KeyedService> BuildFakeGCMProfileService(
    content::BrowserContext* context) {
  return gcm::FakeGCMProfileService::Build(static_cast<Profile*>(context));
}

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  MockInstanceIDDriver(const MockInstanceIDDriver&) = delete;
  MockInstanceIDDriver& operator=(const MockInstanceIDDriver&) = delete;
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID,
               instance_id::InstanceID*(const std::string& app_id));
};

class MockInstanceID : public instance_id::InstanceID {
 public:
  MockInstanceID() : InstanceID("", nullptr) {}
  ~MockInstanceID() override = default;

  MOCK_METHOD5(GetToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    base::TimeDelta time_to_live,
                    std::set<Flags> flags,
                    GetTokenCallback callback));

  MOCK_METHOD3(DeleteToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    DeleteTokenCallback callback));

  void GetID(GetIDCallback callback) override { NOTIMPLEMENTED(); }
  void GetCreationTime(GetCreationTimeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

 protected:
  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteIDImpl(DeleteIDCallback callback) override { NOTIMPLEMENTED(); }
};

}  // namespace

class BinaryFCMServiceTest : public ::testing::Test {
 public:
  BinaryFCMServiceTest() {
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildFakeGCMProfileService));

    binary_fcm_service_ = BinaryFCMService::Create(&profile_);
  }

  Profile* profile() { return &profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BinaryFCMService> binary_fcm_service_;
};

TEST_F(BinaryFCMServiceTest, GetsInstanceID) {
  std::string received_instance_id = BinaryFCMService::kInvalidId;

  // Allow |binary_fcm_service_| to get an instance id.
  content::RunAllTasksUntilIdle();

  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));

  content::RunAllTasksUntilIdle();

  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);
}

TEST_F(BinaryFCMServiceTest, RoutesMessages) {
  enterprise_connectors::ContentAnalysisResponse response1;
  enterprise_connectors::ContentAnalysisResponse response2;

  binary_fcm_service_->SetCallbackForToken(
      "token1",
      base::BindRepeating(
          [](enterprise_connectors::ContentAnalysisResponse* target_response,
             enterprise_connectors::ContentAnalysisResponse response) {
            *target_response = response;
          },
          &response1));
  binary_fcm_service_->SetCallbackForToken(
      "token2",
      base::BindRepeating(
          [](enterprise_connectors::ContentAnalysisResponse* target_response,
             enterprise_connectors::ContentAnalysisResponse response) {
            *target_response = response;
          },
          &response2));

  enterprise_connectors::ContentAnalysisResponse message;
  std::string serialized_message;
  gcm::IncomingMessage incoming_message;

  // Test that a message with token1 is routed only to the first callback.
  message.set_request_token("token1");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  serialized_message = base::Base64Encode(serialized_message);
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.request_token(), "token1");
  EXPECT_EQ(response2.request_token(), "");

  // Test that a message with token2 is routed only to the second callback.
  message.set_request_token("token2");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  serialized_message = base::Base64Encode(serialized_message);
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.request_token(), "token1");
  EXPECT_EQ(response2.request_token(), "token2");

  // Test that I can clear a callback
  response2.clear_request_token();
  binary_fcm_service_->ClearCallbackForToken("token2");
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response2.request_token(), "");
}

TEST_F(BinaryFCMServiceTest, UnregisterToken) {
  // Get an instance ID
  std::string received_instance_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));
  content::RunAllTasksUntilIdle();
  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);

  // Delete it
  bool unregistered = false;
  binary_fcm_service_->UnregisterInstanceID(
      received_instance_id,
      base::BindOnce([](bool* target, bool value) { *target = value; },
                     &unregistered));
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(unregistered);
}

TEST_F(BinaryFCMServiceTest, UnregisterTokenRetriesFailures) {
  // Get an instance ID
  std::string received_instance_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));
  content::RunAllTasksUntilIdle();
  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);

  // Queue one failure, then success
  gcm::FakeGCMProfileService* gcm_service =
      static_cast<gcm::FakeGCMProfileService*>(
          gcm::GCMProfileServiceFactory::GetForProfile(&profile_));
  gcm_service->AddExpectedUnregisterResponse(gcm::GCMClient::NETWORK_ERROR);

  // Delete it
  bool unregistered = false;
  binary_fcm_service_->UnregisterInstanceID(
      received_instance_id,
      base::BindOnce([](bool* target, bool value) { *target = value; },
                     &unregistered));
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(unregistered);
}

TEST_F(BinaryFCMServiceTest, UnregistersTokensOnShutdown) {
  // Get an instance ID
  std::string received_instance_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &received_instance_id));
  content::RunAllTasksUntilIdle();
  EXPECT_NE(received_instance_id, BinaryFCMService::kInvalidId);

  binary_fcm_service_->Shutdown();

  // Shutdown the InstanceID service.
  instance_id::InstanceIDProfileServiceFactory::GetInstance()
      ->SetTestingFactory(&profile_,
                          BrowserContextKeyedServiceFactory::TestingFactory());

  // Ensure we tear down correctly. This used to crash.
}

TEST_F(BinaryFCMServiceTest, UnregisterOneTokensOneCall) {
  NiceMock<MockInstanceIDDriver> driver;
  MockInstanceID instance_id;
  ON_CALL(driver, GetInstanceID).WillByDefault(Return(&instance_id));
  binary_fcm_service_.reset();
  binary_fcm_service_ = std::make_unique<BinaryFCMService>(
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      &driver);

  EXPECT_CALL(instance_id, GetToken)
      .Times(2)
      .WillRepeatedly(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token",
                                    instance_id::InstanceID::Result::SUCCESS);
          }));

  std::string first_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &first_id));

  std::string second_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &second_id));

  content::RunAllTasksUntilIdle();

  EXPECT_CALL(instance_id, DeleteToken)
      .WillOnce(
          Invoke([](const std::string&, const std::string&,
                    instance_id::InstanceID::DeleteTokenCallback callback) {
            std::move(callback).Run(instance_id::InstanceID::Result::SUCCESS);
          }));

  binary_fcm_service_->UnregisterInstanceID(first_id, base::DoNothing());
  binary_fcm_service_->UnregisterInstanceID(second_id, base::DoNothing());

  content::RunAllTasksUntilIdle();
}

TEST_F(BinaryFCMServiceTest, UnregisterTwoTokensTwoCalls) {
  NiceMock<MockInstanceIDDriver> driver;
  MockInstanceID instance_id;
  ON_CALL(driver, GetInstanceID).WillByDefault(Return(&instance_id));
  binary_fcm_service_.reset();
  binary_fcm_service_ = std::make_unique<BinaryFCMService>(
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      &driver);

  EXPECT_CALL(instance_id, GetToken)
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token",
                                    instance_id::InstanceID::Result::SUCCESS);
          }))
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token 2",
                                    instance_id::InstanceID::Result::SUCCESS);
          }));

  std::string first_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &first_id));

  std::string second_id = BinaryFCMService::kInvalidId;
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &second_id));

  content::RunAllTasksUntilIdle();

  EXPECT_CALL(instance_id, DeleteToken)
      .Times(2)
      .WillRepeatedly(
          Invoke([](const std::string&, const std::string&,
                    instance_id::InstanceID::DeleteTokenCallback callback) {
            std::move(callback).Run(instance_id::InstanceID::Result::SUCCESS);
          }));

  binary_fcm_service_->UnregisterInstanceID(first_id, base::DoNothing());
  binary_fcm_service_->UnregisterInstanceID(second_id, base::DoNothing());

  content::RunAllTasksUntilIdle();
}

TEST_F(BinaryFCMServiceTest, UnregisterTwoTokenConflict) {
  NiceMock<MockInstanceIDDriver> driver;
  MockInstanceID instance_id;
  ON_CALL(driver, GetInstanceID).WillByDefault(Return(&instance_id));
  binary_fcm_service_.reset();
  binary_fcm_service_ = std::make_unique<BinaryFCMService>(
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      &driver);
  binary_fcm_service_->SetQueuedOperationDelayForTesting(base::TimeDelta());
  std::string first_id = BinaryFCMService::kInvalidId;
  std::string second_id = BinaryFCMService::kInvalidId;

  // Both calls to GetToken return the same value since we mock a case where the
  // second GetToken call happens before the first DeleteToken call resolves.
  EXPECT_CALL(instance_id, GetToken)
      .Times(2)
      .WillRepeatedly(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token",
                                    instance_id::InstanceID::Result::SUCCESS);
          }));

  EXPECT_CALL(instance_id, DeleteToken)
      .WillOnce(
          Invoke([this, &second_id](
                     const std::string&, const std::string&,
                     instance_id::InstanceID::DeleteTokenCallback callback) {
            // Call the second GetInstanceID here to have a conflict.
            binary_fcm_service_->GetInstanceID(base::BindLambdaForTesting(
                [&second_id](const std::string& instance_id) {
                  second_id = instance_id;
                }));
            std::move(callback).Run(instance_id::InstanceID::Result::SUCCESS);
          }));

  // Get the first token.
  binary_fcm_service_->GetInstanceID(base::BindLambdaForTesting(
      [&first_id](const std::string& instance_id) { first_id = instance_id; }));

  // Get the second token and unregister the first token. This is a conflict as
  // they are the same token.
  binary_fcm_service_->UnregisterInstanceID(
      first_id,
      base::BindOnce([](bool unregister) { EXPECT_TRUE(unregister); }));
  task_environment_.RunUntilIdle();

  // Unregister the second token.
  EXPECT_CALL(instance_id, DeleteToken)
      .WillOnce(
          Invoke([](const std::string&, const std::string&,
                    instance_id::InstanceID::DeleteTokenCallback callback) {
            std::move(callback).Run(instance_id::InstanceID::Result::SUCCESS);
          }));
  binary_fcm_service_->UnregisterInstanceID(
      second_id,
      base::BindOnce([](bool unregister) { EXPECT_TRUE(unregister); }));
  task_environment_.RunUntilIdle();
}

TEST_F(BinaryFCMServiceTest, QueuesGetInstanceIDOnRetriableError) {
  NiceMock<MockInstanceIDDriver> driver;
  MockInstanceID instance_id;
  ON_CALL(driver, GetInstanceID).WillByDefault(Return(&instance_id));
  binary_fcm_service_.reset();
  binary_fcm_service_ = std::make_unique<BinaryFCMService>(
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      &driver);

  EXPECT_CALL(instance_id, GetToken)
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run(
                "", instance_id::InstanceID::Result::ASYNC_OPERATION_PENDING);
          }))
      .WillOnce(
          Invoke([](const std::string&, const std::string&, base::TimeDelta,
                    std::set<instance_id::InstanceID::Flags>,
                    instance_id::InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token",
                                    instance_id::InstanceID::Result::SUCCESS);
          }));

  std::string instance_id_token = BinaryFCMService::kInvalidId;
  binary_fcm_service_->SetQueuedOperationDelayForTesting(base::TimeDelta());
  binary_fcm_service_->GetInstanceID(base::BindOnce(
      [](std::string* target_id, const std::string& instance_id) {
        *target_id = instance_id;
      },
      &instance_id_token));

  content::RunAllTasksUntilIdle();

  EXPECT_NE(instance_id_token, BinaryFCMService::kInvalidId);
}

}  // namespace safe_browsing

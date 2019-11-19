// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/binary_fcm_service.h"

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

std::unique_ptr<KeyedService> BuildFakeGCMProfileService(
    content::BrowserContext* context) {
  return gcm::FakeGCMProfileService::Build(static_cast<Profile*>(context));
}

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
  DeepScanningClientResponse response1;
  DeepScanningClientResponse response2;

  binary_fcm_service_->SetCallbackForToken(
      "token1", base::BindRepeating(
                    [](DeepScanningClientResponse* target_response,
                       DeepScanningClientResponse response) {
                      *target_response = response;
                    },
                    &response1));
  binary_fcm_service_->SetCallbackForToken(
      "token2", base::BindRepeating(
                    [](DeepScanningClientResponse* target_response,
                       DeepScanningClientResponse response) {
                      *target_response = response;
                    },
                    &response2));

  DeepScanningClientResponse message;
  std::string serialized_message;
  gcm::IncomingMessage incoming_message;

  // Test that a message with token1 is routed only to the first callback.
  message.set_token("token1");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  base::Base64Encode(serialized_message, &serialized_message);
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.token(), "token1");
  EXPECT_EQ(response2.token(), "");

  // Test that a message with token2 is routed only to the second callback.
  message.set_token("token2");
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  base::Base64Encode(serialized_message, &serialized_message);
  incoming_message.data["proto"] = serialized_message;
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response1.token(), "token1");
  EXPECT_EQ(response2.token(), "token2");

  // Test that I can clear a callback
  response2.clear_token();
  binary_fcm_service_->ClearCallbackForToken("token2");
  binary_fcm_service_->OnMessage("app_id", incoming_message);
  EXPECT_EQ(response2.token(), "");
}

TEST_F(BinaryFCMServiceTest, EmitsHasKeyHistogram) {
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasKey", false, 1);
  }
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    incoming_message.data["proto"] = "proto";
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasKey", true, 1);
  }
}

TEST_F(BinaryFCMServiceTest, EmitsMessageParsedHistogram) {
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    incoming_message.data["proto"] = "invalid base 64";
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedBase64", false, 1);
  }
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;

    incoming_message.data["proto"] = "invalid+proto+data==";
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedBase64", true, 1);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedProto", false, 1);
  }
  {
    base::HistogramTester histograms;
    gcm::IncomingMessage incoming_message;
    DeepScanningClientResponse message;
    std::string serialized_message;

    ASSERT_TRUE(message.SerializeToString(&serialized_message));
    base::Base64Encode(serialized_message, &serialized_message);
    incoming_message.data["proto"] = serialized_message;
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedBase64", true, 1);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageParsedProto", true, 1);
  }
}

TEST_F(BinaryFCMServiceTest, EmitsMessageHasValidTokenHistogram) {
  gcm::IncomingMessage incoming_message;
  DeepScanningClientResponse message;

  message.set_token("token1");
  std::string serialized_message;
  ASSERT_TRUE(message.SerializeToString(&serialized_message));
  base::Base64Encode(serialized_message, &serialized_message);
  incoming_message.data["proto"] = serialized_message;

  {
    base::HistogramTester histograms;
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasValidToken", false, 1);
  }
  {
    binary_fcm_service_->SetCallbackForToken("token1", base::DoNothing());
    base::HistogramTester histograms;
    binary_fcm_service_->OnMessage("app_id", incoming_message);
    histograms.ExpectUniqueSample(
        "SafeBrowsingFCMService.IncomingMessageHasValidToken", true, 1);
  }
}

}  // namespace safe_browsing

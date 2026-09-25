// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/one_time_tokens/one_time_token_sharing_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/sharing_message/proto/one_time_token_backend_notification.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

class OneTimeTokenSharingHandlerTest : public testing::Test {
 protected:
  OneTimeTokenSharingHandlerTest() = default;
};

TEST_F(OneTimeTokenSharingHandlerTest, OnMessageCallsBackendAndRunsCallback) {
  base::HistogramTester histogram_tester;
  one_time_tokens::FakeGmailOtpBackend fake_gmail_otp_backend;
  auto handler =
      std::make_unique<OneTimeTokenSharingHandler>(&fake_gmail_otp_backend);

  constexpr int64_t kOtpCreatedSeconds = 123456789;
  constexpr int64_t kEmailReceivedSeconds = 987654321;
  constexpr int64_t kEmailDeliveredSeconds = 987654330;

  std::string expected_message_reference = "test_message_reference";
  base::Time expected_otp_created_timestamp =
      base::Time::UnixEpoch() + base::Seconds(kOtpCreatedSeconds);
  base::Time expected_email_received_timestamp =
      base::Time::UnixEpoch() + base::Seconds(kEmailReceivedSeconds);
  base::Time expected_email_delivered_timestamp =
      base::Time::UnixEpoch() + base::Seconds(kEmailDeliveredSeconds);

  components_sharing_message::SharingMessage message;
  components_sharing_message::GmailMessageReference* gmail_otp =
      message.mutable_one_time_token_backend_notification()
          ->mutable_gmail_one_time_password();
  gmail_otp->set_encrypted_message_reference(expected_message_reference);
  gmail_otp->mutable_otp_created_timestamp()->set_seconds(kOtpCreatedSeconds);
  gmail_otp->mutable_email_received_timestamp()->set_seconds(
      kEmailReceivedSeconds);
  gmail_otp->mutable_email_delivered_timestamp()->set_seconds(
      kEmailDeliveredSeconds);

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(_));

  handler->OnMessage(message, done_callback.Get());

  ASSERT_EQ(fake_gmail_otp_backend.incoming_notifications().size(), 1u);
  const one_time_tokens::OneTimeTokenBackendNotification& notification =
      fake_gmail_otp_backend.incoming_notifications().front();
  EXPECT_EQ(notification.otp_created_timestamp, expected_otp_created_timestamp);
  EXPECT_EQ(notification.email_received_timestamp,
            expected_email_received_timestamp);
  EXPECT_EQ(notification.email_delivered_timestamp,
            expected_email_delivered_timestamp);
  EXPECT_EQ(notification.encrypted_message_reference.value(),
            expected_message_reference);

  histogram_tester.ExpectUniqueSample(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      OneTimeTokenValidationResult::kSuccess, 1);
}

TEST_F(OneTimeTokenSharingHandlerTest,
       OnEmptySharingMessageDoesNotCallBackend) {
  base::HistogramTester histogram_tester;
  one_time_tokens::FakeGmailOtpBackend fake_gmail_otp_backend;
  auto handler =
      std::make_unique<OneTimeTokenSharingHandler>(&fake_gmail_otp_backend);

  components_sharing_message::SharingMessage message;
  // Instantiate the one_time_token_backend_notification, but don't initialize
  // the rest of the message.
  message.mutable_one_time_token_backend_notification();

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(_));

  handler->OnMessage(message, done_callback.Get());

  EXPECT_TRUE(fake_gmail_otp_backend.incoming_notifications().empty());

  histogram_tester.ExpectUniqueSample(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      OneTimeTokenValidationResult::kNotGmailOneTimePassword, 1);
}

TEST_F(OneTimeTokenSharingHandlerTest,
       OnEmptyMessageReferenceDoesNotCallBackend) {
  base::HistogramTester histogram_tester;
  one_time_tokens::FakeGmailOtpBackend fake_gmail_otp_backend;
  auto handler =
      std::make_unique<OneTimeTokenSharingHandler>(&fake_gmail_otp_backend);

  components_sharing_message::SharingMessage message;
  // Instantiate the one_time_token_backend_notification, but don't initialize
  // the encrypted_message_reference.
  message.mutable_one_time_token_backend_notification()
      ->mutable_gmail_one_time_password();

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(_));

  handler->OnMessage(message, done_callback.Get());

  EXPECT_TRUE(fake_gmail_otp_backend.incoming_notifications().empty());

  histogram_tester.ExpectUniqueSample(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      OneTimeTokenValidationResult::kEmptyEncryptedMessageReference, 1);
}

}  // namespace

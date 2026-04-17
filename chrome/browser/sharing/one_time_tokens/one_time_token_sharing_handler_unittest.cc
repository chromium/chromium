// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/one_time_tokens/one_time_token_sharing_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/sharing_message/proto/one_time_token_backend_notification.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

class MockGmailOtpBackend : public one_time_tokens::GmailOtpBackend {
 public:
  MOCK_METHOD(one_time_tokens::ExpiringSubscription,
              Subscribe,
              (base::Time expiration, Callback callback),
              (override));

  MOCK_METHOD(void,
              OnIncomingOneTimeTokenBackendNotification,
              (const one_time_tokens::OneTimeTokenBackendNotification&
                   one_time_token_backend_notification),
              (override));
};

MATCHER_P3(OneTimeTokenNotificationMatches,
           expected_otp_created_timestamp,
           expected_email_received_timestamp,
           expected_message_reference,
           "") {
  return arg.otp_created_timestamp == expected_otp_created_timestamp &&
         arg.email_received_timestamp == expected_email_received_timestamp &&
         arg.encrypted_message_reference.value() == expected_message_reference;
}

class OneTimeTokenSharingHandlerTest : public testing::Test {
 protected:
  OneTimeTokenSharingHandlerTest() = default;
};

TEST_F(OneTimeTokenSharingHandlerTest, OnMessageCallsBackendAndRunsCallback) {
  base::HistogramTester histogram_tester;
  MockGmailOtpBackend mock_gmail_otp_backend;
  auto handler =
      std::make_unique<OneTimeTokenSharingHandler>(&mock_gmail_otp_backend);

  std::string expected_message_reference = "test_message_reference";
  base::Time expected_otp_created_timestamp =
      base::Time::UnixEpoch() + base::Seconds(123456789);
  base::Time expected_email_received_timestamp =
      base::Time::UnixEpoch() + base::Seconds(987654321);

  components_sharing_message::SharingMessage message;
  components_sharing_message::GmailMessageReference* gmail_otp =
      message.mutable_one_time_token_backend_notification()
          ->mutable_gmail_one_time_password();
  gmail_otp->set_encrypted_message_reference(expected_message_reference);
  gmail_otp->mutable_otp_created_timestamp()->set_seconds(123456789);
  gmail_otp->mutable_email_received_timestamp()->set_seconds(987654321);

  EXPECT_CALL(
      mock_gmail_otp_backend,
      OnIncomingOneTimeTokenBackendNotification(OneTimeTokenNotificationMatches(
          expected_otp_created_timestamp, expected_email_received_timestamp,
          expected_message_reference)));

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(_));

  handler->OnMessage(message, done_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      OneTimeTokenValidationResult::kSuccess, 1);
}

TEST_F(OneTimeTokenSharingHandlerTest,
       OnEmptySharingMessageDoesNotCallBackend) {
  base::HistogramTester histogram_tester;
  MockGmailOtpBackend mock_gmail_otp_backend;
  auto handler =
      std::make_unique<OneTimeTokenSharingHandler>(&mock_gmail_otp_backend);

  components_sharing_message::SharingMessage message;
  // Instantiate the one_time_token_backend_notification, but don't initialize
  // the rest of the message.
  message.mutable_one_time_token_backend_notification();

  EXPECT_CALL(mock_gmail_otp_backend,
              OnIncomingOneTimeTokenBackendNotification(_))
      .Times(0);

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(_));

  handler->OnMessage(message, done_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      OneTimeTokenValidationResult::kNotGmailOneTimePassword, 1);
}

TEST_F(OneTimeTokenSharingHandlerTest,
       OnEmptyMessageReferenceDoesNotCallBackend) {
  base::HistogramTester histogram_tester;
  MockGmailOtpBackend mock_gmail_otp_backend;
  auto handler =
      std::make_unique<OneTimeTokenSharingHandler>(&mock_gmail_otp_backend);

  components_sharing_message::SharingMessage message;
  // Instantiate the one_time_token_backend_notification, but don't initialize
  // the encrypted_message_reference.
  message.mutable_one_time_token_backend_notification()
      ->mutable_gmail_one_time_password();

  EXPECT_CALL(mock_gmail_otp_backend,
              OnIncomingOneTimeTokenBackendNotification(_))
      .Times(0);

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(_));

  handler->OnMessage(message, done_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      OneTimeTokenValidationResult::kEmptyEncryptedMessageReference, 1);
}

}  // namespace

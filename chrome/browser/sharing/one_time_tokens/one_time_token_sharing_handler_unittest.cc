// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/one_time_tokens/one_time_token_sharing_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
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

  MOCK_METHOD(
      void,
      OnIncomingOneTimeTokenBackendTickle,
      (const one_time_tokens::GmailOtpBackend::EncryptedMessageReference&
           encrypted_message_reference),
      (override));
};

MATCHER_P(OneTimeTokenTickleHasMessageReference,
          expected_message_reference,
          "") {
  return arg.value() == expected_message_reference;
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
  components_sharing_message::SharingMessage message;
  message.mutable_one_time_token_backend_notification()
      ->mutable_gmail_one_time_password()
      ->set_encrypted_message_reference(expected_message_reference);

  EXPECT_CALL(
      mock_gmail_otp_backend,
      OnIncomingOneTimeTokenBackendTickle(
          OneTimeTokenTickleHasMessageReference(expected_message_reference)));

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

  EXPECT_CALL(mock_gmail_otp_backend, OnIncomingOneTimeTokenBackendTickle(_))
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

  EXPECT_CALL(mock_gmail_otp_backend, OnIncomingOneTimeTokenBackendTickle(_))
      .Times(0);

  base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
  EXPECT_CALL(done_callback, Run(_));

  handler->OnMessage(message, done_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      OneTimeTokenValidationResult::kEmptyEncryptedMessageReference, 1);
}

}  // namespace

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_ONE_TIME_TOKENS_ONE_TIME_TOKEN_SHARING_HANDLER_H_
#define CHROME_BROWSER_SHARING_ONE_TIME_TOKENS_ONE_TIME_TOKEN_SHARING_HANDLER_H_

#include "base/functional/callback.h"
#include "components/sharing_message/proto/one_time_token_backend_notification.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"

namespace components_sharing_message {
class SharingMessage;
}  // namespace components_sharing_message

namespace one_time_tokens {
class GmailOtpBackend;
}  // namespace one_time_tokens

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OneTimeTokenValidationResult)
enum class OneTimeTokenValidationResult {
  kSuccess = 0,
  kNotGmailOneTimePassword = 1,
  kEmptyEncryptedMessageReference = 2,
  kMaxValue = kEmptyEncryptedMessageReference,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sharing/enums.xml:OneTimeTokenValidationResult)

}  // namespace

// A SharingMessageHandler that handles OneTimeTokenBackendTickle messages.
// These messages are used to notify the device of a new OTP that is available
// for retrieval from One Time Token backend.
class OneTimeTokenSharingHandler : public SharingMessageHandler {
 public:
  explicit OneTimeTokenSharingHandler(
      one_time_tokens::GmailOtpBackend* gmail_otp_backend);
  ~OneTimeTokenSharingHandler() override;

  void OnMessage(components_sharing_message::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  OneTimeTokenValidationResult HandleOneTimeTokenNotification(
      const components_sharing_message::OneTimeTokenBackendNotification&
          notification);

  raw_ptr<one_time_tokens::GmailOtpBackend> gmail_otp_backend_ = nullptr;
};

#endif  // CHROME_BROWSER_SHARING_ONE_TIME_TOKENS_ONE_TIME_TOKEN_SHARING_HANDLER_H_

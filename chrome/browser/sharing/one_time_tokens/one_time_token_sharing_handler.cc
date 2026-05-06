// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/one_time_tokens/one_time_token_sharing_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/gmail_otp_backend_factory.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/sharing_message/proto/one_time_token_backend_notification.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/proto/timestamp.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "content/public/browser/browser_context.h"

namespace {
base::Time FromSharingProtoTimestamp(
    const components_sharing_message::Timestamp& timestamp) {
  return base::Time::UnixEpoch() + base::Seconds(timestamp.seconds()) +
         base::Nanoseconds(timestamp.nanos());
}
}  // namespace

OneTimeTokenSharingHandler::OneTimeTokenSharingHandler(
    one_time_tokens::GmailOtpBackend* gmail_otp_backend)
    : gmail_otp_backend_(gmail_otp_backend) {}

OneTimeTokenSharingHandler::~OneTimeTokenSharingHandler() = default;

void OneTimeTokenSharingHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  CHECK(message.has_one_time_token_backend_notification());

  OneTimeTokenValidationResult validation_result =
      HandleOneTimeTokenNotification(
          message.one_time_token_backend_notification());
  base::UmaHistogramEnumeration(
      "Sharing.OneTimeTokenSharingHandler.NotificationValidationResult",
      validation_result);

  std::move(done_callback).Run(/*response=*/nullptr);
}

OneTimeTokenValidationResult
OneTimeTokenSharingHandler::HandleOneTimeTokenNotification(
    const components_sharing_message::OneTimeTokenBackendNotification&
        notification) {
  if (!notification.has_gmail_one_time_password()) {
    return OneTimeTokenValidationResult::kNotGmailOneTimePassword;
  }
  const components_sharing_message::GmailMessageReference&
      gmail_message_reference = notification.gmail_one_time_password();
  if (gmail_message_reference.encrypted_message_reference().empty()) {
    return OneTimeTokenValidationResult::kEmptyEncryptedMessageReference;
  }
  gmail_otp_backend_->OnIncomingOneTimeTokenBackendNotification(
      one_time_tokens::OneTimeTokenBackendNotification(
          /*encrypted_message_reference=*/
          one_time_tokens::EncryptedMessageReference(
              gmail_message_reference.encrypted_message_reference()),
          /*otp_created_timestamp=*/
          FromSharingProtoTimestamp(
              gmail_message_reference.otp_created_timestamp()),
          /*email_received_timestamp=*/
          FromSharingProtoTimestamp(
              gmail_message_reference.email_received_timestamp()),
          // `notification_received_timestamp` and
          // `notification_received_timeticks` are measured at the same physical
          // time moment. `notification_received_timeticks` is used for pure
          // client-side usages such as metrics. For more information on the
          // differences between `base::Time` and `base::TimeTicks`, see
          // //base/time/time.h.
          /*notification_received_timestamp=*/base::Time::Now(),
          /*notification_received_timeticks=*/base::TimeTicks::Now()));
  return OneTimeTokenValidationResult::kSuccess;
}

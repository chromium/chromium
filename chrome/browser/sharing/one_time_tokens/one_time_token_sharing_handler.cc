// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/one_time_tokens/one_time_token_sharing_handler.h"

#include "chrome/browser/autofill/gmail_otp_backend_factory.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "content/public/browser/browser_context.h"

OneTimeTokenSharingHandler::OneTimeTokenSharingHandler(
    one_time_tokens::GmailOtpBackend* gmail_otp_backend)
    : gmail_otp_backend_(gmail_otp_backend) {}

OneTimeTokenSharingHandler::~OneTimeTokenSharingHandler() = default;

void OneTimeTokenSharingHandler::OnMessage(
    components_sharing_message::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  CHECK(message.has_one_time_token_backend_notification());
  if (!message.one_time_token_backend_notification()
           .has_gmail_one_time_password() ||
      message.one_time_token_backend_notification()
          .gmail_one_time_password()
          .encrypted_message_reference()
          .empty()) {
    // TODO(crbug.com/482313390): Add logging.
    std::move(done_callback).Run(/*response=*/nullptr);
    return;
  }

  gmail_otp_backend_->OnIncomingOneTimeTokenBackendTickle(
      one_time_tokens::GmailOtpBackend::EncryptedMessageReference(
          message.one_time_token_backend_notification()
              .gmail_one_time_password()
              .encrypted_message_reference()));
  std::move(done_callback).Run(/*response=*/nullptr);
}

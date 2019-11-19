// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_handler.h"

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_handler_registry.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/gcm_driver/gcm_driver.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// The regex captures
// Group 1: type:timesmap
// Group 2: userId#
// Group 3: hashcode
const char kMessageIdRegexPattern[] = "(0:[0-9]+%)([0-9]+#)?([a-f0-9]+)";

// Returns message_id with userId stripped.
// FCM message_id is a persistent id in format of:
//     0:1416811810537717%0#e7a71353318775c7
//     ^          ^       ^          ^
// type :    timestamp % userId # hashcode
// As per go/persistent-id, userId# is optional, and should be stripped
// comparing persistent ids.
// Retrns |message_id| with userId stripped, or |message_id| if it is not
// confined to the format.
std::string GetStrippedMessageId(const std::string& message_id) {
  std::string stripped_message_id, type_timestamp, hashcode;
  static const base::NoDestructor<re2::RE2> kMessageIdRegex(
      kMessageIdRegexPattern);
  if (!re2::RE2::FullMatch(message_id, *kMessageIdRegex, &type_timestamp,
                           nullptr, &hashcode)) {
    return message_id;
  }
  return base::StrCat({type_timestamp, hashcode});
}

}  // namespace

SharingFCMHandler::SharingFCMHandler(
    gcm::GCMDriver* gcm_driver,
    SharingFCMSender* sharing_fcm_sender,
    SharingSyncPreference* sync_preference,
    std::unique_ptr<SharingHandlerRegistry> handler_registry)
    : gcm_driver_(gcm_driver),
      sharing_fcm_sender_(sharing_fcm_sender),
      sync_preference_(sync_preference),
      handler_registry_(std::move(handler_registry)) {}

SharingFCMHandler::~SharingFCMHandler() {
  StopListening();
}

void SharingFCMHandler::StartListening() {
  if (!is_listening_) {
    gcm_driver_->AddAppHandler(kSharingFCMAppID, this);
    is_listening_ = true;
  }
}

void SharingFCMHandler::StopListening() {
  if (is_listening_) {
    gcm_driver_->RemoveAppHandler(kSharingFCMAppID);
    is_listening_ = false;
  }
}

void SharingFCMHandler::OnMessagesDeleted(const std::string& app_id) {
  // TODO: Handle message deleted from the server.
}

void SharingFCMHandler::ShutdownHandler() {
  is_listening_ = false;
}

void SharingFCMHandler::OnMessage(const std::string& app_id,
                                  const gcm::IncomingMessage& message) {
  std::string message_id = GetStrippedMessageId(message.message_id);
  chrome_browser_sharing::SharingMessage sharing_message;
  if (!sharing_message.ParseFromString(message.raw_data)) {
    LOG(ERROR) << "Failed to parse incoming message with id : " << message_id;
    return;
  }
  DCHECK(sharing_message.payload_case() !=
         chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET)
      << "No payload set in SharingMessage received";

  chrome_browser_sharing::MessageType message_type =
      chrome_browser_sharing::UNKNOWN_MESSAGE;
  if (sharing_message.payload_case() ==
      chrome_browser_sharing::SharingMessage::kAckMessage) {
    DCHECK(sharing_message.has_ack_message());
    message_type = sharing_message.ack_message().original_message_type();
  } else {
    message_type =
        SharingPayloadCaseToMessageType(sharing_message.payload_case());
  }
  LogSharingMessageReceived(message_type, sharing_message.payload_case());

  SharingMessageHandler* handler =
      handler_registry_->GetSharingHandler(sharing_message.payload_case());
  if (!handler) {
    LOG(ERROR) << "No handler found for payload : "
               << sharing_message.payload_case();
  } else {
    SharingMessageHandler::DoneCallback done_callback = base::DoNothing();
    if (sharing_message.payload_case() !=
        chrome_browser_sharing::SharingMessage::kAckMessage) {
      done_callback = base::BindOnce(
          &SharingFCMHandler::SendAckMessage, weak_ptr_factory_.GetWeakPtr(),
          std::move(message_id), message_type, GetTargetInfo(sharing_message));
    }

    handler->OnMessage(std::move(sharing_message), std::move(done_callback));
  }
}

void SharingFCMHandler::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  NOTIMPLEMENTED();
}

void SharingFCMHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  NOTIMPLEMENTED();
}

void SharingFCMHandler::OnStoreReset() {
  // TODO: Handle GCM store reset.
}

base::Optional<syncer::DeviceInfo::SharingTargetInfo>
SharingFCMHandler::GetTargetInfo(
    const chrome_browser_sharing::SharingMessage& original_message) {
  if (original_message.has_sender_info()) {
    auto& sender_info = original_message.sender_info();
    return syncer::DeviceInfo::SharingTargetInfo{sender_info.fcm_token(),
                                                 sender_info.p256dh(),
                                                 sender_info.auth_secret()};
  }

  return sync_preference_->GetTargetInfo(original_message.sender_guid());
}

void SharingFCMHandler::SendAckMessage(
    std::string original_message_id,
    chrome_browser_sharing::MessageType original_message_type,
    base::Optional<syncer::DeviceInfo::SharingTargetInfo> target_info,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  if (!target_info) {
    LOG(ERROR) << "Unable to find target info";
    LogSendSharingAckMessageResult(original_message_type,
                                   SharingSendMessageResult::kDeviceNotFound);
    return;
  }

  chrome_browser_sharing::SharingMessage sharing_message;
  chrome_browser_sharing::AckMessage* ack_message =
      sharing_message.mutable_ack_message();
  ack_message->set_original_message_id(original_message_id);
  ack_message->set_original_message_type(original_message_type);
  if (response)
    ack_message->set_allocated_response_message(response.release());

  sharing_fcm_sender_->SendMessageToDevice(
      std::move(*target_info), kAckTimeToLive, std::move(sharing_message),
      base::BindOnce(&SharingFCMHandler::OnAckMessageSent,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(original_message_id), original_message_type));
}

void SharingFCMHandler::OnAckMessageSent(
    std::string original_message_id,
    chrome_browser_sharing::MessageType original_message_type,
    SharingSendMessageResult result,
    base::Optional<std::string> message_id) {
  LogSendSharingAckMessageResult(original_message_type, result);
  if (result != SharingSendMessageResult::kSuccessful)
    LOG(ERROR) << "Failed to send ack mesage for " << original_message_id;
}

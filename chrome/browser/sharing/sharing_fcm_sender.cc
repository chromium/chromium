// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include "base/functional/callback.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_message_bridge.h"
#include "chrome/browser/sharing/sharing_utils.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/local_device_info_provider.h"

SharingFCMSender::SharingFCMSender(
    SharingMessageBridge* sharing_message_bridge,
    gcm::GCMDriver* gcm_driver,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    syncer::SyncService* sync_service)
    : sharing_message_bridge_(sharing_message_bridge),
      gcm_driver_(gcm_driver),
      local_device_info_provider_(local_device_info_provider),
      sync_service_(sync_service) {}

SharingFCMSender::~SharingFCMSender() = default;

void SharingFCMSender::DoSendMessageToDevice(const syncer::DeviceInfo& device,
                                             base::TimeDelta time_to_live,
                                             SharingMessage message,
                                             SendMessageCallback callback) {
  TRACE_EVENT0("sharing", "SharingFCMSender::DoSendMessageToDevice");

  auto fcm_configuration = GetFCMChannel(device);
  if (!fcm_configuration) {
    std::move(callback).Run(SharingSendMessageResult::kDeviceNotFound,
                            /*message_id=*/absl::nullopt,
                            SharingChannelType::kUnknown);
    return;
  }

  if (!SetMessageSenderInfo(&message)) {
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            /*message_id=*/absl::nullopt,
                            SharingChannelType::kUnknown);
    return;
  }

  SendMessageToFcmTarget(*fcm_configuration, time_to_live, std::move(message),
                         std::move(callback));
}

void SharingFCMSender::SendMessageToFcmTarget(
    const chrome_browser_sharing::FCMChannelConfiguration& fcm_configuration,
    base::TimeDelta time_to_live,
    SharingMessage message,
    SendMessageCallback callback) {
  TRACE_EVENT0("sharing", "SharingFCMSender::SendMessageToFcmTarget");

  bool can_send =
      sync_service_->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE) &&
      !fcm_configuration.sender_id_fcm_token().empty() &&
      !fcm_configuration.sender_id_p256dh().empty() &&
      !fcm_configuration.sender_id_auth_secret().empty();
  if (!can_send) {
    std::move(callback).Run(SharingSendMessageResult::kDeviceNotFound,
                            /*message_id=*/absl::nullopt,
                            SharingChannelType::kUnknown);
    return;
  }

  message.set_message_id(base::GenerateGUID());
  EncryptMessage(
      kSharingSenderID, fcm_configuration.sender_id_p256dh(),
      fcm_configuration.sender_id_auth_secret(), message,
      SharingChannelType::kFcmSenderId, std::move(callback),
      base::BindOnce(&SharingFCMSender::DoSendMessageToSenderIdTarget,
                     weak_ptr_factory_.GetWeakPtr(),
                     fcm_configuration.sender_id_fcm_token(), time_to_live,
                     message.message_id()));
}

void SharingFCMSender::SendMessageToServerTarget(
    const chrome_browser_sharing::ServerChannelConfiguration& server_channel,
    SharingMessage message,
    SendMessageCallback callback) {
  TRACE_EVENT0("sharing", "SharingFCMSender::SendMessageToServerTarget");

  if (!sync_service_->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE)) {
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            /*message_id=*/absl::nullopt,
                            SharingChannelType::kServer);
    return;
  }

  message.set_message_id(base::GenerateGUID());
  EncryptMessage(
      kSharingSenderID, server_channel.p256dh(), server_channel.auth_secret(),
      message, SharingChannelType::kServer, std::move(callback),
      base::BindOnce(&SharingFCMSender::DoSendMessageToServerTarget,
                     weak_ptr_factory_.GetWeakPtr(),
                     server_channel.configuration(), message.message_id()));
}

void SharingFCMSender::EncryptMessage(const std::string& authorized_entity,
                                      const std::string& p256dh,
                                      const std::string& auth_secret,
                                      const SharingMessage& message,
                                      SharingChannelType channel_type,
                                      SendMessageCallback callback,
                                      MessageSender message_sender) {
  std::string payload;
  message.SerializeToString(&payload);
  gcm_driver_->EncryptMessage(
      kSharingFCMAppID, authorized_entity, p256dh, auth_secret, payload,
      base::BindOnce(&SharingFCMSender::OnMessageEncrypted,
                     weak_ptr_factory_.GetWeakPtr(), channel_type,
                     std::move(callback), std::move(message_sender)));
}

void SharingFCMSender::OnMessageEncrypted(SharingChannelType channel_type,
                                          SendMessageCallback callback,
                                          MessageSender message_sender,
                                          gcm::GCMEncryptionResult result,
                                          std::string message) {
  if (result != gcm::GCMEncryptionResult::ENCRYPTED_DRAFT_08) {
    LOG(ERROR) << "Unable to encrypt message";
    std::move(callback).Run(SharingSendMessageResult::kEncryptionError,
                            /*message_id=*/absl::nullopt, channel_type);
    return;
  }

  std::move(message_sender).Run(std::move(message), std::move(callback));
}

void SharingFCMSender::DoSendMessageToSenderIdTarget(
    const std::string& fcm_token,
    base::TimeDelta time_to_live,
    const std::string& message_id,
    std::string message,
    SendMessageCallback callback) {
  TRACE_EVENT0("sharing", "SharingFCMSender::DoSendMessageToSenderIdTarget");

  // Double check that SHARING_MESSAGE is syncing.
  if (!sync_service_->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE)) {
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            /*message_id=*/absl::nullopt,
                            SharingChannelType::kFcmSenderId);
    return;
  }

  auto specifics = std::make_unique<sync_pb::SharingMessageSpecifics>();
  auto* fcm_configuration =
      specifics->mutable_channel_configuration()->mutable_fcm();
  fcm_configuration->set_token(fcm_token);
  fcm_configuration->set_ttl(time_to_live.InSeconds());
  fcm_configuration->set_priority(10);
  specifics->set_payload(message);

  sharing_message_bridge_->SendSharingMessage(
      std::move(specifics),
      base::BindOnce(&SharingFCMSender::OnMessageSentViaSync,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     message_id, SharingChannelType::kFcmSenderId));
}

void SharingFCMSender::DoSendMessageToServerTarget(
    const std::string& server_channel,
    const std::string& message_id,
    std::string message,
    SendMessageCallback callback) {
  TRACE_EVENT0("sharing", "SharingFCMSender::DoSendMessageToServerTarget");

  // Double check that SHARING_MESSAGE is syncing.
  if (!sync_service_->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE)) {
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            /*message_id=*/absl::nullopt,
                            SharingChannelType::kServer);
    return;
  }

  auto specifics = std::make_unique<sync_pb::SharingMessageSpecifics>();
  specifics->mutable_channel_configuration()->set_server(server_channel);
  specifics->set_payload(message);

  sharing_message_bridge_->SendSharingMessage(
      std::move(specifics),
      base::BindOnce(&SharingFCMSender::OnMessageSentViaSync,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     message_id, SharingChannelType::kServer));
}

void SharingFCMSender::OnMessageSentViaSync(
    SendMessageCallback callback,
    const std::string& message_id,
    SharingChannelType channel_type,
    const sync_pb::SharingMessageCommitError& error) {
  TRACE_EVENT1("sharing", "SharingFCMSender::OnMessageSentViaSync", "error",
               error.error_code());

  SharingSendMessageResult send_message_result;
  switch (error.error_code()) {
    case sync_pb::SharingMessageCommitError::NONE:
      send_message_result = SharingSendMessageResult::kSuccessful;
      break;
    case sync_pb::SharingMessageCommitError::NOT_FOUND:
      send_message_result = SharingSendMessageResult::kDeviceNotFound;
      break;
    case sync_pb::SharingMessageCommitError::INVALID_ARGUMENT:
      send_message_result = SharingSendMessageResult::kPayloadTooLarge;
      break;
    case sync_pb::SharingMessageCommitError::INTERNAL:
    case sync_pb::SharingMessageCommitError::UNAVAILABLE:
    case sync_pb::SharingMessageCommitError::RESOURCE_EXHAUSTED:
    case sync_pb::SharingMessageCommitError::UNAUTHENTICATED:
    case sync_pb::SharingMessageCommitError::PERMISSION_DENIED:
    case sync_pb::SharingMessageCommitError::SYNC_TURNED_OFF:
    case sync_pb::SharingMessageCommitError::
        DEPRECATED_SYNC_SERVER_OR_AUTH_ERROR:
    case sync_pb::SharingMessageCommitError::SYNC_SERVER_ERROR:
    case sync_pb::SharingMessageCommitError::SYNC_AUTH_ERROR:
      send_message_result = SharingSendMessageResult::kInternalError;
      break;
    case sync_pb::SharingMessageCommitError::SYNC_NETWORK_ERROR:
      send_message_result = SharingSendMessageResult::kNetworkError;
      break;
    case sync_pb::SharingMessageCommitError::SYNC_TIMEOUT:
      send_message_result = SharingSendMessageResult::kCommitTimeout;
      break;
  }

  std::move(callback).Run(send_message_result, message_id, channel_type);
}

bool SharingFCMSender::SetMessageSenderInfo(SharingMessage* message) {
  absl::optional<syncer::DeviceInfo::SharingInfo> sharing_info =
      local_device_info_provider_->GetLocalDeviceInfo()->sharing_info();
  if (!sharing_info)
    return false;

  auto* fcm_configuration = message->mutable_fcm_channel_configuration();
  fcm_configuration->set_vapid_fcm_token(
      sharing_info->vapid_target_info.fcm_token);
  fcm_configuration->set_vapid_p256dh(sharing_info->vapid_target_info.p256dh);
  fcm_configuration->set_vapid_auth_secret(
      sharing_info->vapid_target_info.auth_secret);
  fcm_configuration->set_sender_id_fcm_token(
      sharing_info->sender_id_target_info.fcm_token);
  fcm_configuration->set_sender_id_p256dh(
      sharing_info->sender_id_target_info.p256dh);
  fcm_configuration->set_sender_id_auth_secret(
      sharing_info->sender_id_target_info.auth_secret);
  return true;
}

void SharingFCMSender::SetSharingMessageBridgeForTesting(
    SharingMessageBridge* sharing_message_bridge) {
  sharing_message_bridge_ = sharing_message_bridge;
}

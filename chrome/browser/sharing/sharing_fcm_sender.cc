// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_fcm_sender.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/gcm_driver.h"

SharingFCMSender::SharingFCMSender(gcm::GCMDriver* gcm_driver,
                                   SharingSyncPreference* sync_preference,
                                   VapidKeyManager* vapid_key_manager)
    : gcm_driver_(gcm_driver),
      sync_preference_(sync_preference),
      vapid_key_manager_(vapid_key_manager) {}

SharingFCMSender::~SharingFCMSender() = default;

void SharingFCMSender::SendMessageToDevice(
    syncer::DeviceInfo::SharingTargetInfo target,
    base::TimeDelta time_to_live,
    SharingMessage message,
    SendMessageCallback callback) {
  base::Optional<SharingSyncPreference::FCMRegistration> fcm_registration =
      sync_preference_->GetFCMRegistration();
  if (!fcm_registration) {
    LOG(ERROR) << "Unable to retrieve FCM registration";
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            base::nullopt);
    return;
  }

  auto* vapid_key = vapid_key_manager_->GetOrCreateKey();
  if (!vapid_key) {
    LOG(ERROR) << "Unable to retrieve VAPID key";
    std::move(callback).Run(SharingSendMessageResult::kInternalError,
                            base::nullopt);
    return;
  }

  gcm::WebPushMessage web_push_message;
  web_push_message.time_to_live = time_to_live.InSeconds();
  web_push_message.urgency = gcm::WebPushMessage::Urgency::kHigh;
  message.SerializeToString(&web_push_message.payload);

  gcm_driver_->SendWebPushMessage(
      kSharingFCMAppID, fcm_registration->authorized_entity, target.p256dh,
      target.auth_secret, target.fcm_token, vapid_key,
      std::move(web_push_message),
      base::BindOnce(&SharingFCMSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharingFCMSender::OnMessageSent(SendMessageCallback callback,
                                     gcm::SendWebPushMessageResult result,
                                     base::Optional<std::string> message_id) {
  SharingSendMessageResult send_message_result;
  switch (result) {
    case gcm::SendWebPushMessageResult::kSuccessful:
      send_message_result = SharingSendMessageResult::kSuccessful;
      break;
    case gcm::SendWebPushMessageResult::kDeviceGone:
      send_message_result = SharingSendMessageResult::kDeviceNotFound;
      break;
    case gcm::SendWebPushMessageResult::kNetworkError:
      send_message_result = SharingSendMessageResult::kNetworkError;
      break;
    case gcm::SendWebPushMessageResult::kPayloadTooLarge:
      send_message_result = SharingSendMessageResult::kPayloadTooLarge;
      break;
    case gcm::SendWebPushMessageResult::kEncryptionFailed:
    case gcm::SendWebPushMessageResult::kCreateJWTFailed:
    case gcm::SendWebPushMessageResult::kServerError:
    case gcm::SendWebPushMessageResult::kParseResponseFailed:
    case gcm::SendWebPushMessageResult::kVapidKeyInvalid:
      send_message_result = SharingSendMessageResult::kInternalError;
      break;
  }

  std::move(callback).Run(send_message_result, message_id);
}

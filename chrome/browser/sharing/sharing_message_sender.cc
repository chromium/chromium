// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_sender.h"
#include "base/guid.h"
#include "base/task/post_task.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/sharing_utils.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "content/public/browser/browser_task_traits.h"

SharingMessageSender::SharingMessageSender(
    std::unique_ptr<SharingFCMSender> sharing_fcm_sender,
    SharingSyncPreference* sync_prefs,
    syncer::LocalDeviceInfoProvider* local_device_info_provider)
    : fcm_sender_(std::move(sharing_fcm_sender)),
      sync_prefs_(sync_prefs),
      local_device_info_provider_(local_device_info_provider) {}

SharingMessageSender::~SharingMessageSender() = default;

void SharingMessageSender::SendMessageToDevice(
    const std::string& device_guid,
    base::TimeDelta response_timeout,
    chrome_browser_sharing::SharingMessage message,
    ResponseCallback callback) {
  DCHECK(message.payload_case() !=
         chrome_browser_sharing::SharingMessage::kAckMessage);

  std::string message_guid = base::GenerateGUID();
  send_message_callbacks_.emplace(message_guid, std::move(callback));
  chrome_browser_sharing::MessageType message_type =
      SharingPayloadCaseToMessageType(message.payload_case());

  base::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, content::BrowserThread::UI},
      base::BindOnce(&SharingMessageSender::InvokeSendMessageCallback,
                     weak_ptr_factory_.GetWeakPtr(), message_guid, message_type,
                     SharingSendMessageResult::kAckTimeout,
                     /*response=*/nullptr),
      response_timeout);

  // TODO(crbug/1015411): Here we assume caller gets |device_guid| from
  // GetDeviceCandidates, so both DeviceInfoTracker and LocalDeviceInfoProvider
  // are already ready. It's better to queue up the message and wait until
  // DeviceInfoTracker and LocalDeviceInfoProvider are ready.
  auto target_info = sync_prefs_->GetTargetInfo(device_guid);
  if (!target_info) {
    InvokeSendMessageCallback(message_guid, message_type,
                              SharingSendMessageResult::kDeviceNotFound,
                              /*response=*/nullptr);
    return;
  }

  const syncer::DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  if (!local_device_info) {
    InvokeSendMessageCallback(message_guid, message_type,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return;
  }

  base::Optional<syncer::DeviceInfo::SharingInfo> sharing_info =
      sync_prefs_->GetLocalSharingInfo(local_device_info);
  if (!sharing_info) {
    InvokeSendMessageCallback(message_guid, message_type,
                              SharingSendMessageResult::kInternalError,
                              /*response=*/nullptr);
    return;
  }

  message.set_sender_guid(local_device_info->guid());
  message.set_sender_device_name(
      GetSharingDeviceNames(local_device_info).full_name);

  auto* sender_info = message.mutable_sender_info();
  sender_info->set_fcm_token(sharing_info->vapid_target_info.fcm_token);
  sender_info->set_p256dh(sharing_info->vapid_target_info.p256dh);
  sender_info->set_auth_secret(sharing_info->vapid_target_info.auth_secret);

  DCHECK_GE(response_timeout, kAckTimeToLive);
  fcm_sender_->SendMessageToDevice(
      std::move(*target_info), response_timeout - kAckTimeToLive,
      std::move(message),
      base::BindOnce(&SharingMessageSender::OnMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     message_guid, message_type));
}

void SharingMessageSender::OnMessageSent(
    base::TimeTicks start_time,
    const std::string& message_guid,
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result,
    base::Optional<std::string> message_id) {
  if (result != SharingSendMessageResult::kSuccessful) {
    InvokeSendMessageCallback(message_guid, message_type, result,
                              /*response=*/nullptr);
    return;
  }

  send_message_times_.emplace(*message_id, start_time);
  message_guids_.emplace(*message_id, message_guid);
}

void SharingMessageSender::OnAckReceived(
    chrome_browser_sharing::MessageType message_type,
    const std::string& message_id,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  auto times_iter = send_message_times_.find(message_id);
  if (times_iter != send_message_times_.end()) {
    LogSharingMessageAckTime(message_type,
                             base::TimeTicks::Now() - times_iter->second);
    send_message_times_.erase(times_iter);
  }

  auto iter = message_guids_.find(message_id);
  if (iter == message_guids_.end())
    return;

  std::string message_guid = std::move(iter->second);
  message_guids_.erase(iter);
  InvokeSendMessageCallback(message_guid, message_type,
                            SharingSendMessageResult::kSuccessful,
                            std::move(response));
}

void SharingMessageSender::InvokeSendMessageCallback(
    const std::string& message_guid,
    chrome_browser_sharing::MessageType message_type,
    SharingSendMessageResult result,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  auto iter = send_message_callbacks_.find(message_guid);
  if (iter == send_message_callbacks_.end())
    return;

  ResponseCallback callback = std::move(iter->second);
  send_message_callbacks_.erase(iter);
  std::move(callback).Run(result, std::move(response));
  LogSendSharingMessageResult(message_type, result);
}

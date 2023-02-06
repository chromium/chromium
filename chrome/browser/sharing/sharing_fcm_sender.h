// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_
#define CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "components/sync_device_info/device_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gcm {
class GCMDriver;
enum class GCMEncryptionResult;
}  // namespace gcm

namespace syncer {
class LocalDeviceInfoProvider;
class SyncService;
}  // namespace syncer

namespace sync_pb {
class SharingMessageCommitError;
}

enum class SharingChannelType;
class SharingMessageBridge;

// Responsible for sending FCM messages within Sharing infrastructure.
class SharingFCMSender : public SharingMessageSender::SendMessageDelegate {
 public:
  using SharingMessage = chrome_browser_sharing::SharingMessage;
  using SendMessageCallback =
      base::OnceCallback<void(SharingSendMessageResult result,
                              absl::optional<std::string> message_id,
                              SharingChannelType channel_type)>;

  SharingFCMSender(SharingMessageBridge* sharing_message_bridge,
                   gcm::GCMDriver* gcm_driver,
                   syncer::LocalDeviceInfoProvider* local_device_info_provider,
                   syncer::SyncService* sync_service);
  SharingFCMSender(const SharingFCMSender&) = delete;
  SharingFCMSender& operator=(const SharingFCMSender&) = delete;
  ~SharingFCMSender() override;

  // Sends a |message| to device identified by |fcm_configuration|, which
  // expires after |time_to_live| seconds. |callback| will be invoked with
  // message_id if asynchronous operation succeeded, or absl::nullopt if
  // operation failed.
  virtual void SendMessageToFcmTarget(
      const chrome_browser_sharing::FCMChannelConfiguration& fcm_configuration,
      base::TimeDelta time_to_live,
      SharingMessage message,
      SendMessageCallback callback);

  // Sends a |message| to device identified by |server_channel|, |callback| will
  // be invoked with message_id if asynchronous operation succeeded, or
  // absl::nullopt if operation failed.
  virtual void SendMessageToServerTarget(
      const chrome_browser_sharing::ServerChannelConfiguration& server_channel,
      SharingMessage message,
      SendMessageCallback callback);

  // Used to inject fake SharingMessageBridge in integration tests.
  void SetSharingMessageBridgeForTesting(
      SharingMessageBridge* sharing_message_bridge);

 protected:
  // SharingMessageSender::SendMessageDelegate:
  void DoSendMessageToDevice(const syncer::DeviceInfo& device,
                             base::TimeDelta time_to_live,
                             SharingMessage message,
                             SendMessageCallback callback) override;

 private:
  using MessageSender = base::OnceCallback<void(std::string message,
                                                SendMessageCallback callback)>;

  void EncryptMessage(const std::string& authorized_entity,
                      const std::string& p256dh,
                      const std::string& auth_secret,
                      const SharingMessage& message,
                      SharingChannelType channel_type,
                      SendMessageCallback callback,
                      MessageSender message_sender);

  void OnMessageEncrypted(SharingChannelType channel_type,
                          SendMessageCallback callback,
                          MessageSender message_sender,
                          gcm::GCMEncryptionResult result,
                          std::string message);

  void DoSendMessageToSenderIdTarget(const std::string& fcm_token,
                                     base::TimeDelta time_to_live,
                                     const std::string& message_id,
                                     std::string message,
                                     SendMessageCallback callback);

  void DoSendMessageToServerTarget(const std::string& server_channel,
                                   const std::string& message_id,
                                   std::string message,
                                   SendMessageCallback callback);

  void OnMessageSentViaSync(SendMessageCallback callback,
                            const std::string& message_id,
                            SharingChannelType channel_type,
                            const sync_pb::SharingMessageCommitError& error);

  bool SetMessageSenderInfo(SharingMessage* message);

  raw_ptr<SharingMessageBridge, DanglingUntriaged> sharing_message_bridge_;
  raw_ptr<gcm::GCMDriver, DanglingUntriaged> gcm_driver_;
  raw_ptr<syncer::LocalDeviceInfoProvider, DanglingUntriaged>
      local_device_info_provider_;
  raw_ptr<syncer::SyncService, DanglingUntriaged> sync_service_;

  base::WeakPtrFactory<SharingFCMSender> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_FCM_SENDER_H_

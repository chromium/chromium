// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_FCM_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARING_FCM_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_send_message_result.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/sync_device_info/device_info.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace syncer {
class DeviceInfoTracker;
}  // namespace syncer

enum class SharingChannelType;
class SharingFCMSender;
class SharingHandlerRegistry;

enum class SharingDevicePlatform;

// SharingFCMHandler is responsible for receiving SharingMessage from GCMDriver
// and delegate it to the payload specific handler.
class SharingFCMHandler : public gcm::GCMAppHandler {
 public:
  SharingFCMHandler(gcm::GCMDriver* gcm_driver,
                    syncer::DeviceInfoTracker* device_info_tracker,
                    SharingFCMSender* sharing_fcm_sender,
                    SharingHandlerRegistry* handler_registry);
  ~SharingFCMHandler() override;

  // Registers itself as app handler for sharing messages.
  virtual void StartListening();

  // Unregisters itself as app handler for sharing messages.
  virtual void StopListening();

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;

  // Responsible for delegating the message to the registered
  // SharingMessageHandler. Also sends back an ack to original sender after
  // delegating the message.
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;

  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;

  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

  void OnMessagesDeleted(const std::string& app_id) override;

 private:
  base::Optional<chrome_browser_sharing::FCMChannelConfiguration> GetFCMChannel(
      const chrome_browser_sharing::SharingMessage& original_message);

  base::Optional<chrome_browser_sharing::ServerChannelConfiguration>
  GetServerChannel(
      const chrome_browser_sharing::SharingMessage& original_message);

  SharingDevicePlatform GetSenderPlatform(
      const chrome_browser_sharing::SharingMessage& original_message);

  // Ack message sent back to the original sender of message.
  void SendAckMessage(
      std::string original_message_id,
      chrome_browser_sharing::MessageType original_message_type,
      base::Optional<chrome_browser_sharing::FCMChannelConfiguration>
          fcm_channel,
      base::Optional<chrome_browser_sharing::ServerChannelConfiguration>
          server_channel,
      SharingDevicePlatform sender_device_type,
      base::TimeTicks message_received_time,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

  void OnAckMessageSent(
      std::string original_message_id,
      chrome_browser_sharing::MessageType original_message_type,
      SharingDevicePlatform sender_device_type,
      int trace_id,
      SharingSendMessageResult result,
      base::Optional<std::string> message_id,
      SharingChannelType channel_type);

  gcm::GCMDriver* const gcm_driver_;
  syncer::DeviceInfoTracker* device_info_tracker_;
  SharingFCMSender* sharing_fcm_sender_;
  SharingHandlerRegistry* handler_registry_;

  bool is_listening_ = false;

  base::WeakPtrFactory<SharingFCMHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingFCMHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_FCM_HANDLER_H_

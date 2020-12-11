// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_SENDER_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_SENDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace chrome_browser_sharing {
enum MessageType : int;
class ResponseMessage;
class SharingMessage;
}  // namespace chrome_browser_sharing

namespace syncer {
class DeviceInfo;
class LocalDeviceInfoProvider;
}  // namespace syncer

enum class SharingChannelType;
class SharingFCMSender;
enum class SharingDevicePlatform;
enum class SharingSendMessageResult;

class SharingMessageSender {
 public:
  using ResponseCallback = base::OnceCallback<void(
      SharingSendMessageResult,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage>)>;

  // Delegate class used to swap the actual message sending implementation.
  class SendMessageDelegate {
   public:
    using SendMessageCallback =
        base::OnceCallback<void(SharingSendMessageResult result,
                                base::Optional<std::string> message_id,
                                SharingChannelType channel_type)>;
    virtual ~SendMessageDelegate() = default;

    virtual void DoSendMessageToDevice(
        const syncer::DeviceInfo& device,
        base::TimeDelta time_to_live,
        chrome_browser_sharing::SharingMessage message,
        SendMessageCallback callback) = 0;
  };

  // Delegate type used to send a message.
  enum class DelegateType {
    kFCM,
    kWebRtc,
  };

  SharingMessageSender(
      syncer::LocalDeviceInfoProvider* local_device_info_provider);
  SharingMessageSender(const SharingMessageSender&) = delete;
  SharingMessageSender& operator=(const SharingMessageSender&) = delete;
  virtual ~SharingMessageSender();

  virtual void SendMessageToDevice(
      const syncer::DeviceInfo& device,
      base::TimeDelta response_timeout,
      chrome_browser_sharing::SharingMessage message,
      DelegateType delegate_type,
      ResponseCallback callback);

  virtual void OnAckReceived(
      const std::string& message_id,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

  // Registers the given |delegate| to send messages when SendMessageToDevice is
  // called with |type|.
  void RegisterSendDelegate(DelegateType type,
                            std::unique_ptr<SendMessageDelegate> delegate);

  // Returns SharingFCMSender for testing.
  SharingFCMSender* GetFCMSenderForTesting() const;

 private:
  struct SentMessageMetadata {
    SentMessageMetadata(ResponseCallback callback,
                        base::TimeTicks timestamp,
                        chrome_browser_sharing::MessageType type,
                        SharingDevicePlatform receiver_device_platform,
                        base::TimeDelta last_updated_age,
                        int trace_id,
                        SharingChannelType channel_type,
                        base::TimeDelta receiver_pulse_interval);
    SentMessageMetadata(SentMessageMetadata&& other);
    SentMessageMetadata& operator=(SentMessageMetadata&& other);
    ~SentMessageMetadata();

    ResponseCallback callback;
    base::TimeTicks timestamp;
    chrome_browser_sharing::MessageType type;
    SharingDevicePlatform receiver_device_platform;
    base::TimeDelta last_updated_age;
    int trace_id;
    SharingChannelType channel_type;
    base::TimeDelta receiver_pulse_interval;
  };

  void OnMessageSent(const std::string& message_guid,
                     SharingSendMessageResult result,
                     base::Optional<std::string> message_id,
                     SharingChannelType channel_type);

  void InvokeSendMessageCallback(
      const std::string& message_guid,
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

  syncer::LocalDeviceInfoProvider* local_device_info_provider_;

  // Map of random GUID to SentMessageMetadata.
  std::map<std::string, SentMessageMetadata> message_metadata_;
  // Map of FCM message_id to random GUID.
  std::map<std::string, std::string> message_guids_;
  // Map of FCM message_id to received ACK response messages.
  std::map<std::string,
           std::unique_ptr<chrome_browser_sharing::ResponseMessage>>
      cached_ack_response_messages_;

  // Registered delegates to send messages.
  std::map<DelegateType, std::unique_ptr<SendMessageDelegate>> send_delegates_;

  base::WeakPtrFactory<SharingMessageSender> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_SENDER_H_

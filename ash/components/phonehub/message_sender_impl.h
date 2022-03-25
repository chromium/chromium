// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_
#define ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_

#include "ash/components/phonehub/message_sender.h"

#include <stdint.h>
#include <string>

#include "ash/components/phonehub/proto/phonehub_api.pb.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace phonehub {

class MessageSenderImpl : public MessageSender {
 public:
  explicit MessageSenderImpl(
      secure_channel::ConnectionManager* connection_manager);
  ~MessageSenderImpl() override;

  // MessageSender:
  void SendCrosState(bool notification_setting_enabled,
                     bool camera_roll_setting_enabled) override;
  void SendUpdateNotificationModeRequest(bool do_not_disturb_enabled) override;
  void SendUpdateBatteryModeRequest(bool battery_saver_mode_enabled) override;
  void SendDismissNotificationRequest(int64_t notification_id) override;
  void SendNotificationInlineReplyRequest(
      int64_t notification_id,
      const std::u16string& reply_text) override;
  void SendShowNotificationAccessSetupRequest() override;
  void SendFeatureSetupRequest(bool camera_roll, bool notifications) override;
  void SendRingDeviceRequest(bool device_ringing_enabled) override;
  void SendFetchCameraRollItemsRequest(
      const proto::FetchCameraRollItemsRequest& request) override;
  void SendFetchCameraRollItemDataRequest(
      const proto::FetchCameraRollItemDataRequest& request) override;
  void SendInitiateCameraRollItemTransferRequest(
      const proto::InitiateCameraRollItemTransferRequest& request) override;

 private:
  void SendMessage(proto::MessageType message_type,
                   const google::protobuf::MessageLite* request);

  secure_channel::ConnectionManager* connection_manager_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_

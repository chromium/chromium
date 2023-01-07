// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_MOCK_SHARING_MESSAGE_SENDER_H_
#define CHROME_BROWSER_SHARING_MOCK_SHARING_MESSAGE_SENDER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSharingMessageSender : public SharingMessageSender {
 public:
  MockSharingMessageSender();
  MockSharingMessageSender(const MockSharingMessageSender&) = delete;
  MockSharingMessageSender& operator=(const MockSharingMessageSender&) = delete;
  ~MockSharingMessageSender() override;

  MOCK_METHOD5(SendMessageToDevice,
               base::OnceClosure(const syncer::DeviceInfo&,
                                 base::TimeDelta,
                                 chrome_browser_sharing::SharingMessage,
                                 DelegateType,
                                 ResponseCallback));

  MOCK_METHOD2(
      OnAckReceived,
      void(const std::string& fcm_message_id,
           std::unique_ptr<chrome_browser_sharing::ResponseMessage> response));
};

#endif  // CHROME_BROWSER_SHARING_MOCK_SHARING_MESSAGE_SENDER_H_

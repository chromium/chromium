// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class SharingMessageSender;

// Class to managae ack message and notify observers.
class AckMessageHandler : public SharingMessageHandler {
 public:
  explicit AckMessageHandler(SharingMessageSender* sharing_message_sender);

  AckMessageHandler(const AckMessageHandler&) = delete;
  AckMessageHandler& operator=(const AckMessageHandler&) = delete;

  ~AckMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  raw_ptr<SharingMessageSender> sharing_message_sender_;
};

#endif  // CHROME_BROWSER_SHARING_ACK_MESSAGE_HANDLER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_PING_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_PING_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class PingMessageHandler : public SharingMessageHandler {
 public:
  PingMessageHandler();
  ~PingMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PingMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_PING_MESSAGE_HANDLER_H_

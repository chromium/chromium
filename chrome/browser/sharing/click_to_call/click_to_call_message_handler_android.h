// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_MESSAGE_HANDLER_ANDROID_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_MESSAGE_HANDLER_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

// Handles incoming messages for the click to call feature.
class ClickToCallMessageHandler : public SharingMessageHandler {
 public:
  ClickToCallMessageHandler();
  ~ClickToCallMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClickToCallMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_MESSAGE_HANDLER_ANDROID_H_

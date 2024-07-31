// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_MESSAGE_HANDLER_ANDROID_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_MESSAGE_HANDLER_ANDROID_H_

#include "components/sharing_message/sharing_message_handler.h"

// Handles incoming messages for the click to call feature.
class ClickToCallMessageHandler : public SharingMessageHandler {
 public:
  ClickToCallMessageHandler();

  ClickToCallMessageHandler(const ClickToCallMessageHandler&) = delete;
  ClickToCallMessageHandler& operator=(const ClickToCallMessageHandler&) =
      delete;

  ~ClickToCallMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(components_sharing_message::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 protected:
  // Calls into Java to handle a |phone_number|. Virtual for testing.
  virtual void HandlePhoneNumber(const std::string& phone_number);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_MESSAGE_HANDLER_ANDROID_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/sharing/sharing_message_handler.h"

class SharingDeviceSource;

// Handles incoming messages for the shared clipboard feature.
class SharedClipboardMessageHandler : public SharingMessageHandler {
 public:
  explicit SharedClipboardMessageHandler(SharingDeviceSource* device_source);
  ~SharedClipboardMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(chrome_browser_sharing::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 protected:
  // Called after the message has been copied to the clipboard. Implementers
  // should display a notification using |device_name|.
  virtual void ShowNotification(const std::string& device_name) = 0;

 private:
  SharingDeviceSource* device_source_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardMessageHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_H_

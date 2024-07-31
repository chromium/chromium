// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sharing_message/sharing_message_handler.h"

class SharingDeviceSource;

// Handles incoming messages for the shared clipboard feature.
class SharedClipboardMessageHandler : public SharingMessageHandler {
 public:
  explicit SharedClipboardMessageHandler(SharingDeviceSource* device_source);

  SharedClipboardMessageHandler(const SharedClipboardMessageHandler&) = delete;
  SharedClipboardMessageHandler& operator=(
      const SharedClipboardMessageHandler&) = delete;

  ~SharedClipboardMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(components_sharing_message::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 protected:
  // Called after the message has been copied to the clipboard. Implementers
  // should display a notification using |device_name|.
  virtual void ShowNotification(const std::string& device_name) = 0;

 private:
  raw_ptr<SharingDeviceSource> device_source_ = nullptr;
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_H_

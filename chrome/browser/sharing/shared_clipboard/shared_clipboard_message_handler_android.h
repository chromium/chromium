// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler.h"

class SharingDeviceSource;

class SharedClipboardMessageHandlerAndroid
    : public SharedClipboardMessageHandler {
 public:
  explicit SharedClipboardMessageHandlerAndroid(
      SharingDeviceSource* device_source);

  SharedClipboardMessageHandlerAndroid(
      const SharedClipboardMessageHandlerAndroid&) = delete;
  SharedClipboardMessageHandlerAndroid& operator=(
      const SharedClipboardMessageHandlerAndroid&) = delete;

  ~SharedClipboardMessageHandlerAndroid() override;

 private:
  // SharedClipboardMessageHandler implementation.
  void ShowNotification(const std::string& device_name) override;
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_

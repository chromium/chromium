// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler.h"

class SharingDeviceSource;

class SharedClipboardMessageHandlerAndroid
    : public SharedClipboardMessageHandler {
 public:
  explicit SharedClipboardMessageHandlerAndroid(
      SharingDeviceSource* device_source);
  ~SharedClipboardMessageHandlerAndroid() override;

 private:
  // SharedClipboardMessageHandler implementation.
  void ShowNotification(const std::string& device_name) override;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardMessageHandlerAndroid);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_ANDROID_H_

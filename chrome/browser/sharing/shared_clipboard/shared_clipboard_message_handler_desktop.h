// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_DESKTOP_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_DESKTOP_H_

#include "base/macros.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler.h"

class Profile;
class SharingDeviceSource;

// Handles incoming messages for the shared clipboard feature.
class SharedClipboardMessageHandlerDesktop
    : public SharedClipboardMessageHandler {
 public:
  SharedClipboardMessageHandlerDesktop(SharingDeviceSource* device_source,
                                       Profile* profile);
  ~SharedClipboardMessageHandlerDesktop() override;

 private:
  // SharedClipboardMessageHandler implementation.
  void ShowNotification(const std::string& device_name) override;

  Profile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardMessageHandlerDesktop);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_DESKTOP_H_

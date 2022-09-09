// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_DESKTOP_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler.h"

class Profile;
class SharingDeviceSource;

// Handles incoming messages for the shared clipboard feature.
class SharedClipboardMessageHandlerDesktop
    : public SharedClipboardMessageHandler {
 public:
  SharedClipboardMessageHandlerDesktop(SharingDeviceSource* device_source,
                                       Profile* profile);

  SharedClipboardMessageHandlerDesktop(
      const SharedClipboardMessageHandlerDesktop&) = delete;
  SharedClipboardMessageHandlerDesktop& operator=(
      const SharedClipboardMessageHandlerDesktop&) = delete;

  ~SharedClipboardMessageHandlerDesktop() override;

 private:
  // SharedClipboardMessageHandler implementation.
  void ShowNotification(const std::string& device_name) override;

  raw_ptr<Profile> profile_ = nullptr;
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_MESSAGE_HANDLER_DESKTOP_H_

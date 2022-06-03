// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_H_
#define CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_H_

#include "chrome/browser/sharing/proto/sharing_message.pb.h"

class SharingMessageHandler;

class SharingHandlerRegistry {
 public:
  SharingHandlerRegistry() = default;
  virtual ~SharingHandlerRegistry() = default;

  // Gets SharingMessageHandler registered for |payload_case|.
  virtual SharingMessageHandler* GetSharingHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case) = 0;

  // Register SharingMessageHandler for |payload_case|.
  virtual void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case) = 0;

  // Unregister SharingMessageHandler for |payload_case|.
  virtual void UnregisterSharingHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case) = 0;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_H_

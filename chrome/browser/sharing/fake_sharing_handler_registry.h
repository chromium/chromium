// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_FAKE_SHARING_HANDLER_REGISTRY_H_
#define CHROME_BROWSER_SHARING_FAKE_SHARING_HANDLER_REGISTRY_H_

#include <map>

#include "chrome/browser/sharing/sharing_handler_registry.h"

class FakeSharingHandlerRegistry : public SharingHandlerRegistry {
 public:
  FakeSharingHandlerRegistry();
  FakeSharingHandlerRegistry(const FakeSharingHandlerRegistry&) = delete;
  FakeSharingHandlerRegistry& operator=(const FakeSharingHandlerRegistry&) =
      delete;
  ~FakeSharingHandlerRegistry() override;

  // SharingHandlerRegistry:
  SharingMessageHandler* GetSharingHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case)
      override;
  void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case)
      override;
  void UnregisterSharingHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case)
      override;

  void SetSharingHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case,
      SharingMessageHandler* handler);

 private:
  std::map<chrome_browser_sharing::SharingMessage::PayloadCase,
           SharingMessageHandler*>
      handler_map_;
};

#endif  // CHROME_BROWSER_SHARING_FAKE_SHARING_HANDLER_REGISTRY_H_

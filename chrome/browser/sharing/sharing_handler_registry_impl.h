// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_IMPL_H_
#define CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "chrome/browser/sharing/sharing_handler_registry.h"

namespace content {
class SmsFetcher;
}  // namespace content

class SharingMessageHandler;
class SharingDeviceRegistration;
class SharingDeviceSource;
class SharingMessageSender;
class Profile;

// Interface for handling incoming SharingMessage.
class SharingHandlerRegistryImpl : public SharingHandlerRegistry {
 public:
  SharingHandlerRegistryImpl(
      Profile* profile,
      SharingDeviceRegistration* sharing_device_registration,
      SharingMessageSender* message_sender,
      SharingDeviceSource* device_source,
      content::SmsFetcher* sms_fetcher);
  ~SharingHandlerRegistryImpl() override;

  // Gets SharingMessageHandler registered for |payload_case|.
  SharingMessageHandler* GetSharingHandler(
      chrome_browser_sharing::SharingMessage::PayloadCase payload_case)
      override;

 private:
  // Registers |handler| for handling |payload_cases| SharingMessages.
  void AddSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      std::set<chrome_browser_sharing::SharingMessage::PayloadCase>
          payload_cases);

  std::vector<std::unique_ptr<SharingMessageHandler>> handlers_;
  std::map<chrome_browser_sharing::SharingMessage::PayloadCase,
           SharingMessageHandler*>
      handler_map_;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_IMPL_H_

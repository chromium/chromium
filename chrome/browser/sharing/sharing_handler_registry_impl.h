// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_IMPL_H_
#define CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sharing_message/sharing_handler_registry.h"

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
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;

  // Register SharingMessageHandler for |payload_case|.
  void RegisterSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;

  // Unregister SharingMessageHandler for |payload_case|.
  void UnregisterSharingHandler(
      components_sharing_message::SharingMessage::PayloadCase payload_case)
      override;

 private:
  // Registers |handler| for handling |payload_cases| SharingMessages. No
  // handlers should have been registered with |payload_cases|.
  void AddSharingHandler(
      std::unique_ptr<SharingMessageHandler> handler,
      std::set<components_sharing_message::SharingMessage::PayloadCase>
          payload_cases);

 private:
  std::vector<std::unique_ptr<SharingMessageHandler>> handlers_;
  std::map<components_sharing_message::SharingMessage::PayloadCase,
           raw_ptr<SharingMessageHandler, CtnExperimental>>
      handler_map_;
  std::map<components_sharing_message::SharingMessage::PayloadCase,
           std::unique_ptr<SharingMessageHandler>>
      extra_handler_map_;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_HANDLER_REGISTRY_IMPL_H_

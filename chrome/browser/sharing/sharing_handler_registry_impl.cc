// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_handler_registry_impl.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sharing/optimization_guide/optimization_guide_message_handler.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/sharing_message/ack_message_handler.h"
#include "components/sharing_message/ping_message_handler.h"
#include "components/sharing_message/sharing_device_registration.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "components/sharing_message/sharing_message_sender.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"
#include "chrome/browser/sharing/sms/sms_fetch_request_handler.h"
#else
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS)) BUILDFLAG(IS_CHROMEOS)

SharingHandlerRegistryImpl::SharingHandlerRegistryImpl(
    Profile* profile,
    SharingDeviceRegistration* sharing_device_registration,
    SharingMessageSender* message_sender,
    SharingDeviceSource* device_source,
    content::SmsFetcher* sms_fetcher) {
  AddSharingHandler(std::make_unique<PingMessageHandler>(),
                    {components_sharing_message::SharingMessage::kPingMessage});

  AddSharingHandler(std::make_unique<AckMessageHandler>(message_sender),
                    {components_sharing_message::SharingMessage::kAckMessage});

#if BUILDFLAG(IS_ANDROID)
  // Note: IsClickToCallSupported() is not used as it requires JNI call.
  AddSharingHandler(
      std::make_unique<ClickToCallMessageHandler>(),
      {components_sharing_message::SharingMessage::kClickToCallMessage});

  if (sharing_device_registration->IsSmsFetcherSupported()) {
    AddSharingHandler(
        std::make_unique<SmsFetchRequestHandler>(device_source, sms_fetcher),
        {components_sharing_message::SharingMessage::kSmsFetchRequest});
  }

#endif  // BUILDFLAG(IS_ANDROID)

  // Profile can be null in tests.
  if (optimization_guide::features::IsPushNotificationsEnabled() &&
      optimization_guide::features::IsOptimizationHintsEnabled() && profile) {
    AddSharingHandler(OptimizationGuideMessageHandler::Create(profile),
                      {components_sharing_message::SharingMessage::
                           kOptimizationGuidePushNotification});
  }

#if !BUILDFLAG(IS_ANDROID)
  if (sharing_device_registration->IsSharedClipboardSupported()) {
    std::unique_ptr<SharingMessageHandler> shared_clipboard_message_handler;
    shared_clipboard_message_handler =
        std::make_unique<SharedClipboardMessageHandlerDesktop>(device_source,
                                                               profile);
    AddSharingHandler(
        std::move(shared_clipboard_message_handler),
        {components_sharing_message::SharingMessage::kSharedClipboardMessage});
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (sharing_device_registration->IsRemoteCopySupported()) {
    AddSharingHandler(
        std::make_unique<RemoteCopyMessageHandler>(profile),
        {components_sharing_message::SharingMessage::kRemoteCopyMessage});
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

SharingHandlerRegistryImpl::~SharingHandlerRegistryImpl() = default;

SharingMessageHandler* SharingHandlerRegistryImpl::GetSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  auto it = handler_map_.find(payload_case);
  if (it != handler_map_.end())
    return it->second;

  auto extra_it = extra_handler_map_.find(payload_case);
  if (extra_it != extra_handler_map_.end())
    return extra_it->second.get();

  return nullptr;
}

void SharingHandlerRegistryImpl::AddSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    std::set<components_sharing_message::SharingMessage::PayloadCase>
        payload_cases) {
  DCHECK(handler) << "Received request to add null handler";

  for (const auto& payload_case : payload_cases) {
    DCHECK(payload_case !=
           components_sharing_message::SharingMessage::PAYLOAD_NOT_SET)
        << "Incorrect payload type specified for handler";
    DCHECK(!handler_map_.count(payload_case)) << "Handler already exists";
    handler_map_[payload_case] = handler.get();
  }

  handlers_.push_back(std::move(handler));
}

void SharingHandlerRegistryImpl::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  DCHECK(handler) << "Received request to add null handler";
  DCHECK(!GetSharingHandler(payload_case));
  DCHECK(payload_case !=
         components_sharing_message::SharingMessage::PAYLOAD_NOT_SET)
      << "Incorrect payload type specified for handler";

  extra_handler_map_[payload_case] = std::move(handler);
}

void SharingHandlerRegistryImpl::UnregisterSharingHandler(
    components_sharing_message::SharingMessage::PayloadCase payload_case) {
  extra_handler_map_.erase(payload_case);
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_handler_registry_impl.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sharing/ack_message_handler.h"
#include "chrome/browser/sharing/optimization_guide/optimization_guide_message_handler.h"
#include "chrome/browser/sharing/ping_message_handler.h"
#include "chrome/browser/sharing/sharing_device_registration.h"
#include "chrome/browser/sharing/sharing_message_handler.h"
#include "chrome/browser/sharing/sharing_message_sender.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_android.h"
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
                    {chrome_browser_sharing::SharingMessage::kPingMessage});

  AddSharingHandler(std::make_unique<AckMessageHandler>(message_sender),
                    {chrome_browser_sharing::SharingMessage::kAckMessage});

#if BUILDFLAG(IS_ANDROID)
  // Note: IsClickToCallSupported() is not used as it requires JNI call.
  AddSharingHandler(
      std::make_unique<ClickToCallMessageHandler>(),
      {chrome_browser_sharing::SharingMessage::kClickToCallMessage});

  if (sharing_device_registration->IsSmsFetcherSupported()) {
    AddSharingHandler(
        std::make_unique<SmsFetchRequestHandler>(device_source, sms_fetcher),
        {chrome_browser_sharing::SharingMessage::kSmsFetchRequest});
  }

#endif  // BUILDFLAG(IS_ANDROID)

  if (optimization_guide::features::IsPushNotificationsEnabled() &&
      optimization_guide::features::IsOptimizationHintsEnabled()) {
    AddSharingHandler(OptimizationGuideMessageHandler::Create(profile),
                      {chrome_browser_sharing::SharingMessage::
                           kOptimizationGuidePushNotification});
  }

  if (sharing_device_registration->IsSharedClipboardSupported()) {
    std::unique_ptr<SharingMessageHandler> shared_clipboard_message_handler;
#if BUILDFLAG(IS_ANDROID)
    shared_clipboard_message_handler =
        std::make_unique<SharedClipboardMessageHandlerAndroid>(device_source);
#else
    shared_clipboard_message_handler =
        std::make_unique<SharedClipboardMessageHandlerDesktop>(device_source,
                                                               profile);
#endif  // BUILDFLAG(IS_ANDROID)
    AddSharingHandler(
        std::move(shared_clipboard_message_handler),
        {chrome_browser_sharing::SharingMessage::kSharedClipboardMessage});
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (sharing_device_registration->IsRemoteCopySupported()) {
    AddSharingHandler(
        std::make_unique<RemoteCopyMessageHandler>(profile),
        {chrome_browser_sharing::SharingMessage::kRemoteCopyMessage});
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

SharingHandlerRegistryImpl::~SharingHandlerRegistryImpl() = default;

SharingMessageHandler* SharingHandlerRegistryImpl::GetSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
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
    std::set<chrome_browser_sharing::SharingMessage::PayloadCase>
        payload_cases) {
  DCHECK(handler) << "Received request to add null handler";

  for (const auto& payload_case : payload_cases) {
    DCHECK(payload_case !=
           chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET)
        << "Incorrect payload type specified for handler";
    DCHECK(!handler_map_.count(payload_case)) << "Handler already exists";
    handler_map_[payload_case] = handler.get();
  }

  handlers_.push_back(std::move(handler));
}

void SharingHandlerRegistryImpl::RegisterSharingHandler(
    std::unique_ptr<SharingMessageHandler> handler,
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  DCHECK(handler) << "Received request to add null handler";
  DCHECK(!GetSharingHandler(payload_case));
  DCHECK(payload_case !=
         chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET)
      << "Incorrect payload type specified for handler";

  extra_handler_map_[payload_case] = std::move(handler);
}

void SharingHandlerRegistryImpl::UnregisterSharingHandler(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  extra_handler_map_.erase(payload_case);
}

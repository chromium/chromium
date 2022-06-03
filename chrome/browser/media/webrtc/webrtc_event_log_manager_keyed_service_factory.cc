// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service_factory.h"

#include "base/check.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace webrtc_event_logging {

// static
WebRtcEventLogManagerKeyedServiceFactory*
WebRtcEventLogManagerKeyedServiceFactory::GetInstance() {
  return base::Singleton<WebRtcEventLogManagerKeyedServiceFactory>::get();
}

WebRtcEventLogManagerKeyedServiceFactory::
    WebRtcEventLogManagerKeyedServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WebRtcEventLogManagerKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

WebRtcEventLogManagerKeyedServiceFactory::
    ~WebRtcEventLogManagerKeyedServiceFactory() = default;

bool WebRtcEventLogManagerKeyedServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* WebRtcEventLogManagerKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());
  return new WebRtcEventLogManagerKeyedService(context);
}

}  // namespace webrtc_event_logging

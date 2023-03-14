// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service_factory.h"

#include "base/check.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace webrtc_event_logging {

// static
WebRtcEventLogManagerKeyedServiceFactory*
WebRtcEventLogManagerKeyedServiceFactory::GetInstance() {
  return base::Singleton<WebRtcEventLogManagerKeyedServiceFactory>::get();
}

WebRtcEventLogManagerKeyedServiceFactory::
    WebRtcEventLogManagerKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebRtcEventLogManagerKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

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

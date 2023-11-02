// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_KEYED_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}  // namespace content

namespace webrtc_event_logging {

// Produces WebRtcEventLogManagerKeyedService-s for non-incognito profiles.
class WebRtcEventLogManagerKeyedServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static WebRtcEventLogManagerKeyedServiceFactory* GetInstance();

  WebRtcEventLogManagerKeyedServiceFactory(
      const WebRtcEventLogManagerKeyedServiceFactory&) = delete;
  WebRtcEventLogManagerKeyedServiceFactory& operator=(
      const WebRtcEventLogManagerKeyedServiceFactory&) = delete;

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::DefaultSingletonTraits<
      WebRtcEventLogManagerKeyedServiceFactory>;

  WebRtcEventLogManagerKeyedServiceFactory();
  ~WebRtcEventLogManagerKeyedServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_KEYED_SERVICE_FACTORY_H_

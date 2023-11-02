// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_KEYED_SERVICE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_KEYED_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace webrtc_event_logging {

// KeyedService working on behalf of WebRtcEventLogManager, informing it when
// new BrowserContext-s are loaded.
class WebRtcEventLogManagerKeyedService : public KeyedService {
 public:
  explicit WebRtcEventLogManagerKeyedService(
      content::BrowserContext* browser_context);

  WebRtcEventLogManagerKeyedService(const WebRtcEventLogManagerKeyedService&) =
      delete;
  WebRtcEventLogManagerKeyedService& operator=(
      const WebRtcEventLogManagerKeyedService&) = delete;

  ~WebRtcEventLogManagerKeyedService() override = default;

  void Shutdown() override;

 private:
  // The BrowserContext associated with this instance of the service.
  const raw_ptr<content::BrowserContext> browser_context_;

  // Whether the singleton content::WebRtcEventLogger existed at the time this
  // service was instantiated, and therefore got the report that this
  // BrowserContext was loaded.
  // See usage for rationale.
  bool reported_;
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_MANAGER_KEYED_SERVICE_H_

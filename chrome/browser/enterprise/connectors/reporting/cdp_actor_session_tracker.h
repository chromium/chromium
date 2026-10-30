// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_

#include <optional>

#include "net/base/ip_endpoint.h"

namespace content {
class BrowserContext;
class DevToolsAgentHostClientChannel;
}  // namespace content

namespace enterprise_connectors {

// Classifies CDP client channels and checks browser-context eligibility for
// enterprise security event attribution.
class CdpActorSessionTracker {
 public:
  enum class SessionCategory {
    kIgnored,
    kDevToolsUi,
    kExternalPipe,
    kExternalWebSocket,
  };

  struct ClassifiedClient {
    SessionCategory category = SessionCategory::kIgnored;
    std::optional<net::IPEndPoint> remote_endpoint;
  };

  CdpActorSessionTracker() = delete;
  CdpActorSessionTracker(const CdpActorSessionTracker&) = delete;
  CdpActorSessionTracker& operator=(const CdpActorSessionTracker&) = delete;

  // Classifies `channel` into an ignored client ("Extension" / internal
  // "Other"), built-in DevTools UI ("DevTools"), external Pipe
  // (TransportType::kPipe), or external WebSocket (TransportType::kWebSocket).
  // Returns `SessionCategory::kIgnored` if `channel` or its client is null.
  static ClassifiedClient ClassifyChannel(
      content::DevToolsAgentHostClientChannel* channel);

  // Returns true if `browser_context` is non-null, maps to a non-OffTheRecord
  // and non-system Profile on a CBCM-managed device (valid browser DM token),
  // and allows reporting device information under active connector scoping.
  static bool IsEligibleBrowserContext(
      content::BrowserContext* browser_context);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_

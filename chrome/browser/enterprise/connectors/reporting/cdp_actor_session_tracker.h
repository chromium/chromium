// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "net/base/ip_endpoint.h"

namespace content {
class BrowserContext;
class DevToolsAgentHostClientChannel;
class WebContents;
}  // namespace content

namespace enterprise_connectors {

// Tracks DevTools session attachments, first CDP command dispatches, and
// detachments across the browser, updating per-tab `TabCdpActorTracker`
// buffers.
//
// Instances are sequence-affine (`SEQUENCE_CHECKER`) and must be constructed,
// called, and destroyed on the same sequence (the browser UI thread).
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

  CdpActorSessionTracker();
  CdpActorSessionTracker(const CdpActorSessionTracker&) = delete;
  CdpActorSessionTracker& operator=(const CdpActorSessionTracker&) = delete;
  ~CdpActorSessionTracker();

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

  // Builds `EventInitiatorInfo` for the outermost tab containing `web_contents`
  // from its `TabCdpActorTracker` when the browser context is eligible. Returns
  // `std::nullopt` if `web_contents` is null, has no recorded actor, or belongs
  // to an ineligible context.
  static std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
  GetEventInitiatorInfo(content::WebContents* web_contents);

  // Tracks a newly attached DevTools client `channel` when it targets an
  // eligible tab-bound `WebContents`, immediately recording built-in DevTools
  // UI sessions. Does nothing if `channel` is null. A non-null `channel` stored
  // in `active_channels_` is retained solely as an opaque lookup key (never
  // dereferenced after `OnClientAttached()` returns) until removed by
  // `OnClientDetached(channel)`.
  void OnClientAttached(content::DevToolsAgentHostClientChannel* channel);

  // Records an attached external Pipe or WebSocket session on its target tab
  // upon its first handled CDP command. Does nothing if `channel` is null, not
  // tracked in `active_channels_`, or has already handled a command. `channel`
  // is used only as an opaque map key and is not dereferenced.
  void OnCommandHandled(content::DevToolsAgentHostClientChannel* channel);

  // Removes `channel` from `active_channels_` and detaches any recorded actor
  // state from its target tab. Does nothing if `channel` is null or untracked.
  // `channel` is used only as an opaque map key and is not dereferenced.
  void OnClientDetached(content::DevToolsAgentHostClientChannel* channel);

 private:
  struct TrackedChannel {
    TrackedChannel() = delete;
    TrackedChannel(SessionCategory category,
                   std::optional<net::IPEndPoint> remote_endpoint,
                   base::WeakPtr<content::WebContents> web_contents);
    TrackedChannel(const TrackedChannel&) = delete;
    TrackedChannel& operator=(const TrackedChannel&) = delete;
    TrackedChannel(TrackedChannel&&) noexcept;
    TrackedChannel& operator=(TrackedChannel&&) noexcept;
    ~TrackedChannel();

    SessionCategory category = SessionCategory::kIgnored;
    std::optional<net::IPEndPoint> remote_endpoint;
    base::WeakPtr<content::WebContents> web_contents;
    bool has_handled_command = false;
  };

  SEQUENCE_CHECKER(sequence_checker_);

  // Active attached channels mapped to their tracked target state for command
  // activation and cleanup on `OnClientDetached()`.
  base::flat_map<const content::DevToolsAgentHostClientChannel*, TrackedChannel>
      active_channels_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_

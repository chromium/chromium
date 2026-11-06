// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "net/base/ip_endpoint.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
class DevToolsAgentHostClientChannel;
class WebContents;
}  // namespace content

namespace enterprise_connectors {

// Tracks DevTools session attachments, CDP command dispatches, and detachments
// across the browser, owns serialized background OS process tree extractions
// with bounded queue shedding, caches resolved `ProcessTree`s for Pipe and
// WebSocket connections, and updates per-tab `TabCdpActorTracker` buffers.
//
// Instances are sequence-affine (`SEQUENCE_CHECKER`) and must be constructed,
// called, and destroyed on the same sequence (the browser UI thread).
class CdpActorSessionTracker {
 public:
  // Callbacks invoked on `extraction_task_runner_` to extract the OS
  // `ProcessTree` for Pipe and WebSocket CDP sessions. Guaranteed to remain
  // `base::RepeatingCallback` aliases so callers may bind and pass custom
  // extractors via `SetExtractorsForTesting()`.
  using PipeExtractorCallback = base::RepeatingCallback<ProcessTree()>;
  using WebSocketExtractorCallback =
      base::RepeatingCallback<ProcessTree(const net::IPEndPoint&)>;

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

  // Creates a `CdpActorSessionTracker` if `kEventReportingCdpAttribution` is
  // enabled, or returns `nullptr` if the feature is disabled.
  static std::unique_ptr<CdpActorSessionTracker> CreateIfEnabled();

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
  // UI sessions and pre-warming the OS process tree cache for external Pipe or
  // WebSocket sessions (without recording a tab actor until `OnCommandHandled`
  // fires). Does nothing if `channel` is null. A non-null `channel` stored in
  // `active_channels_` is retained solely as an opaque lookup key (never
  // dereferenced after `OnClientAttached()` returns) until removed by
  // `OnClientDetached(channel)`.
  void OnClientAttached(content::DevToolsAgentHostClientChannel* channel);

  // Records or refreshes an attached external Pipe or WebSocket session on its
  // target tab upon handling a CDP command. Does nothing if `channel` is null
  // or not tracked in `active_channels_`. `channel` is used only as an opaque
  // map key and is not dereferenced.
  void OnCommandHandled(content::DevToolsAgentHostClientChannel* channel);

  // Removes `channel` from `active_channels_`, detaching built-in DevTools UI
  // state from its target tab or cleaning up unused WebSocket endpoint cache
  // entries. Does nothing if `channel` is null or untracked. `channel` is used
  // only as an opaque map key and is not dereferenced.
  void OnClientDetached(content::DevToolsAgentHostClientChannel* channel);

  // Test hooks for injecting custom OS process tree extractors, a custom task
  // runner, and inspecting internal cache/queue state.
  void SetExtractorsForTesting(
      PipeExtractorCallback pipe_extractor,
      WebSocketExtractorCallback websocket_extractor,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr);
  size_t GetPendingExtractionsCountForTesting() const;
  bool HasCachedPipeProcessTreeForTesting() const;
  bool HasCachedWebSocketProcessTreeForTesting(
      const net::IPEndPoint& endpoint) const;

 private:
  CdpActorSessionTracker();

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

  using WaitingTab =
      std::pair<base::WeakPtr<content::WebContents>, base::TimeTicks>;

  struct ProcessTreeCacheEntry {
    ProcessTreeCacheEntry();
    ProcessTreeCacheEntry(const ProcessTreeCacheEntry&) = delete;
    ProcessTreeCacheEntry& operator=(const ProcessTreeCacheEntry&) = delete;
    ProcessTreeCacheEntry(ProcessTreeCacheEntry&&) noexcept;
    ProcessTreeCacheEntry& operator=(ProcessTreeCacheEntry&&) noexcept;
    ~ProcessTreeCacheEntry();

    std::optional<ProcessTree> cached_tree;
    bool extraction_in_flight = false;
    std::vector<WaitingTab> waiting_tabs;
  };

  void RecordExternalSessionCommand(TrackedChannel& tracked,
                                    content::WebContents* web_contents);

  void StartExtraction(std::optional<net::IPEndPoint> remote_endpoint);
  void OnExtractionCompleted(std::optional<net::IPEndPoint> remote_endpoint,
                             ProcessTree tree);

  bool HasActiveWebSocketChannel(const net::IPEndPoint& endpoint) const;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> extraction_task_runner_;
  PipeExtractorCallback pipe_extractor_;
  WebSocketExtractorCallback websocket_extractor_;

  size_t pending_extractions_count_ = 0;

  // Pipe process tree is cached for the lifetime of the browser once resolved.
  ProcessTreeCacheEntry pipe_cache_;

  // WebSocket process trees are cached per remote `net::IPEndPoint` while at
  // least one channel in `active_channels_` uses that endpoint.
  base::flat_map<net::IPEndPoint, ProcessTreeCacheEntry> websocket_cache_;

  // Active attached channels mapped to their tracked target state for command
  // activation and cleanup on `OnClientDetached()`.
  base::flat_map<const content::DevToolsAgentHostClientChannel*, TrackedChannel>
      active_channels_;

  base::WeakPtrFactory<CdpActorSessionTracker> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_CDP_ACTOR_SESSION_TRACKER_H_

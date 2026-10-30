// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/ip_endpoint.h"

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_connectors {

// Bounded buffer of active and recently detached CDP actors for a target tab's
// WebContents, with detached entries evicted first when at capacity.
//
// Maintains up to `kCdpAttributionMaxActorsPerTab` unique external actors
// (`TRANSPORT_TYPE_PIPE` or `TRANSPORT_TYPE_WEBSOCKET`), deduplicated by
// transport + endpoint or leaf PID, with session reference counting and
// post-detach retention for `kCdpAttributionDetachedActorTtl`. Also tracks
// active built-in DevTools UI (`FEATURE_TYPE_DEVTOOLS_UI`) attachments.
class CdpActorBuffer {
 public:
  // Represents a tracked external CDP actor in the buffer, holding its
  // transport metadata, resolved OS process tree, and active session refcount.
  struct ExternalActorEntry {
    ExternalActorEntry();
    ExternalActorEntry(
        chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
            transport_type,
        std::optional<net::IPEndPoint> peer_endpoint,
        ProcessTree process_tree);
    ExternalActorEntry(const ExternalActorEntry&);
    ExternalActorEntry& operator=(const ExternalActorEntry&);
    ExternalActorEntry(ExternalActorEntry&&) noexcept;
    ExternalActorEntry& operator=(ExternalActorEntry&&) noexcept;
    ~ExternalActorEntry();

    bool IsAttached() const { return active_sessions > 0; }

    chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
        transport_type = chrome::cros::reporting::proto::ExternalProcessInfo::
            TRANSPORT_TYPE_UNSPECIFIED;
    std::optional<net::IPEndPoint> peer_endpoint;
    base::ProcessId leaf_pid = base::kNullProcessId;
    ProcessTree process_tree;
    size_t active_sessions = 0;
    std::optional<base::TimeTicks> detached_at;
  };

  CdpActorBuffer();
  ~CdpActorBuffer();

  CdpActorBuffer(const CdpActorBuffer&) = delete;
  CdpActorBuffer& operator=(const CdpActorBuffer&) = delete;

  // Records an external CDP session attachment (`PIPE` or `WEBSOCKET`).
  // Deduplicates against existing entries and increments `active_sessions`, or
  // inserts a new entry (evicting the oldest detached entry first, or oldest
  // attached entry if full).
  void OnExternalActorAttached(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint = std::nullopt,
      ProcessTree process_tree = {});

  // Records an external CDP session attachment for the given `transport_type`
  // and `process_tree` without a socket endpoint.
  void OnExternalActorAttached(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      ProcessTree process_tree);

  // Decrements `active_sessions` for the matching external actor; when the
  // count reaches 0, records `detached_at = base::TimeTicks::Now()` for the
  // post-detach grace window.
  void OnExternalActorDetached(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint = std::nullopt,
      base::ProcessId pid = base::kNullProcessId);

  // Decrements `active_sessions` for the external actor matching
  // `transport_type` and `pid` without a socket endpoint.
  void OnExternalActorDetached(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      base::ProcessId pid);

  // Updates the `ProcessTree` of a matching external actor in the buffer when
  // an asynchronous OS process tree extraction completes.
  void UpdateExternalActorProcessTree(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint,
      ProcessTree process_tree);

  // Records a built-in DevTools UI (`"DevTools"`) attachment or detachment.
  void OnDevToolsUiAttached();
  void OnDevToolsUiDetached();

  // Lazily prunes expired detached entries and builds `EventInitiatorInfo`.
  // External CDP actors (`PIPE` / `WEBSOCKET`) take precedence over built-in
  // DevTools UI (`FEATURE_TYPE_DEVTOOLS_UI`) because `EventInitiatorInfo.actor`
  // is a `oneof`. Returns `std::nullopt` if no external actor is currently
  // attached or within the post-detach TTL window and no DevTools UI session is
  // active.
  std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
  BuildEventInitiatorInfo();

  // Prunes expired entries and returns the current external actor entries.
  const std::vector<ExternalActorEntry>& GetExternalActorsForTesting();

 private:
  // Prunes detached entries whose age exceeds the post-detach TTL.
  void PruneExpiredEntries(base::TimeTicks now);

  // Returns an iterator to the entry matching `transport_type`,
  // `peer_endpoint`, or `leaf_pid`, or `external_actors_.end()` if no match is
  // found.
  std::vector<ExternalActorEntry>::iterator FindMatchingActor(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint,
      base::ProcessId leaf_pid);

  // Evicts the oldest detached entry, or the oldest attached entry if all
  // entries are active.
  void EvictOneEntryForInsert();

  std::vector<ExternalActorEntry> external_actors_;
  size_t devtools_ui_active_sessions_ = 0;
};

// Per-tab `WebContentsUserData` wrapper around `CdpActorBuffer`.
class TabCdpActorTracker
    : public content::WebContentsUserData<TabCdpActorTracker> {
 public:
  ~TabCdpActorTracker() override;

  TabCdpActorTracker(const TabCdpActorTracker&) = delete;
  TabCdpActorTracker& operator=(const TabCdpActorTracker&) = delete;

  // Returns `EventInitiatorInfo` if `web_contents` has an active or
  // within-TTL CDP actor (and `kEventReportingCdpAttribution` is enabled), or
  // `std::nullopt` otherwise.
  static std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
  GetEventInitiatorInfo(content::WebContents* web_contents);

  CdpActorBuffer& buffer() { return buffer_; }
  const CdpActorBuffer& buffer() const { return buffer_; }

 private:
  friend class content::WebContentsUserData<TabCdpActorTracker>;

  explicit TabCdpActorTracker(content::WebContents* web_contents);

  CdpActorBuffer buffer_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_

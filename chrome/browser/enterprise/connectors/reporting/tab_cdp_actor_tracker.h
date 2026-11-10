// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/ip_endpoint.h"

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_connectors {

// Bounded buffer of external and built-in CDP actors for a target tab's
// WebContents.
//
// Maintains up to `kCdpAttributionMaxActorsPerTab` unique external actors
// (`TRANSPORT_TYPE_PIPE` or `TRANSPORT_TYPE_WEBSOCKET`), deduplicated by
// transport + endpoint or leaf PID, with attribution retention for
// `kCdpAttributionActorIdleTtl` from the last handled CDP command
// (`last_command_at`). Also tracks active built-in DevTools UI
// (`FEATURE_TYPE_DEVTOOLS_UI`) attachments.
class CdpActorBuffer {
 public:
  // Represents a tracked external CDP actor in the buffer, holding its
  // transport metadata, resolved OS process tree, and last command timestamp.
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

    bool IsWithinTtl(base::TimeTicks now) const {
      return now - last_command_at <
             std::max(base::TimeDelta(), kCdpAttributionActorIdleTtl.Get());
    }

    chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
        transport_type = chrome::cros::reporting::proto::ExternalProcessInfo::
            TRANSPORT_TYPE_UNSPECIFIED;
    std::optional<net::IPEndPoint> peer_endpoint;
    base::ProcessId leaf_pid = base::kNullProcessId;
    ProcessTree process_tree;
    base::TimeTicks last_command_at;
  };

  CdpActorBuffer();
  ~CdpActorBuffer();

  CdpActorBuffer(const CdpActorBuffer&) = delete;
  CdpActorBuffer& operator=(const CdpActorBuffer&) = delete;

  // Records an external CDP actor (`PIPE` or `WEBSOCKET`) upon handling a
  // command. Deduplicates against existing entries and refreshes
  // `last_command_at`, or inserts a new entry (evicting the entry with the
  // oldest `last_command_at` if at capacity).
  void OnExternalActorCommandHandled(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint = std::nullopt,
      const ProcessTree& process_tree = {});

  // Records an external CDP actor for the given `transport_type` and
  // `process_tree` without a socket endpoint.
  void OnExternalActorCommandHandled(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      const ProcessTree& process_tree);

  // Updates the `ProcessTree` of a matching external actor in the buffer when
  // an asynchronous OS process tree extraction completes.
  void UpdateExternalActorProcessTree(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint,
      const ProcessTree& process_tree);

  // Records a built-in DevTools UI (`"DevTools"`) attachment or detachment.
  void OnDevToolsUiAttached();
  void OnDevToolsUiDetached();

  // Lazily prunes expired entries and builds `EventInitiatorInfo` from
  // external actors within the command TTL (`IsWithinTtl()`), deduplicating
  // resolved external actors that share the same `(transport_type, leaf_pid)`.
  // External CDP actors (`PIPE` / `WEBSOCKET`) take precedence over built-in
  // DevTools UI (`FEATURE_TYPE_DEVTOOLS_UI`) because
  // `EventInitiatorInfo.initiator` is a `oneof`. Returns `std::nullopt` if no
  // external actor is within the command TTL window and no DevTools UI session
  // is active.
  std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
  BuildEventInitiatorInfo();

  // Prunes expired entries and returns the current external actor entries.
  const std::vector<ExternalActorEntry>& GetExternalActorsForTesting();

  const std::vector<ExternalActorEntry>& external_actors() const {
    return external_actors_;
  }

  bool HasDevToolsUiActive() const { return devtools_ui_active_sessions_ > 0; }

 private:
  // Prunes entries whose time since `last_command_at` exceeds the TTL.
  void PruneExpiredEntries(base::TimeTicks now);

  // Returns an iterator to the entry matching `transport_type`,
  // `peer_endpoint`, or `leaf_pid`, or `external_actors_.end()` if no match is
  // found.
  std::vector<ExternalActorEntry>::iterator FindMatchingActor(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint,
      base::ProcessId leaf_pid);

  // Evicts the entry with the oldest `last_command_at`.
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

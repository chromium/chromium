// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "base/process/process_handle.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "net/base/ip_endpoint.h"

namespace enterprise_connectors {

// Buffer of active external CDP actors (`TRANSPORT_TYPE_PIPE` or
// `TRANSPORT_TYPE_WEBSOCKET`) for a target tab, deduplicated by transport +
// endpoint or leaf PID with session reference counting.
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
  };

  CdpActorBuffer();
  ~CdpActorBuffer();

  CdpActorBuffer(const CdpActorBuffer&) = delete;
  CdpActorBuffer& operator=(const CdpActorBuffer&) = delete;

  // Records an external CDP session attachment (`PIPE` or `WEBSOCKET`).
  // Deduplicates against existing entries and increments `active_sessions`, or
  // appends a new entry.
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

  // Decrements `active_sessions` for the matching external actor and removes
  // the entry when `active_sessions` reaches 0.
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

  // Builds `EventInitiatorInfo` from active external CDP actors (`PIPE` or
  // `WEBSOCKET`). Returns `std::nullopt` if no actor is currently attached.
  std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
  BuildEventInitiatorInfo() const;

  // Returns the current external actor entries.
  const std::vector<ExternalActorEntry>& GetExternalActorsForTesting() const;

 private:
  // Returns an iterator to the entry matching `transport_type`,
  // `peer_endpoint`, or `leaf_pid`, or `external_actors_.end()` if no match is
  // found.
  std::vector<ExternalActorEntry>::iterator FindMatchingActor(
      chrome::cros::reporting::proto::ExternalProcessInfo::TransportType
          transport_type,
      std::optional<net::IPEndPoint> peer_endpoint,
      base::ProcessId leaf_pid);

  std::vector<ExternalActorEntry> external_actors_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TAB_CDP_ACTOR_TRACKER_H_

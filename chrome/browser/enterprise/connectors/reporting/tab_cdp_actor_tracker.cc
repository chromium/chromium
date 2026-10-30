// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_endpoint.h"

namespace enterprise_connectors {

namespace {

using TransportType =
    chrome::cros::reporting::proto::ExternalProcessInfo::TransportType;

base::ProcessId GetLeafPid(const ProcessTree& process_tree) {
  return process_tree.process_nodes.empty()
             ? base::kNullProcessId
             : process_tree.process_nodes.front().process_id;
}

size_t GetMaxActorsPerTab() {
  return static_cast<size_t>(std::max(1, kCdpAttributionMaxActorsPerTab.Get()));
}

base::TimeDelta GetDetachedActorTtl() {
  return std::max(base::TimeDelta(), kCdpAttributionDetachedActorTtl.Get());
}

}  // namespace

CdpActorBuffer::ExternalActorEntry::ExternalActorEntry() = default;

CdpActorBuffer::ExternalActorEntry::ExternalActorEntry(
    TransportType transport_type,
    std::optional<net::IPEndPoint> peer_endpoint,
    ProcessTree process_tree)
    : transport_type(transport_type),
      peer_endpoint(std::move(peer_endpoint)),
      leaf_pid(GetLeafPid(process_tree)),
      process_tree(std::move(process_tree)),
      active_sessions(1) {}

CdpActorBuffer::ExternalActorEntry::ExternalActorEntry(
    const ExternalActorEntry&) = default;
CdpActorBuffer::ExternalActorEntry&
CdpActorBuffer::ExternalActorEntry::operator=(const ExternalActorEntry&) =
    default;
CdpActorBuffer::ExternalActorEntry::ExternalActorEntry(
    ExternalActorEntry&&) noexcept = default;
CdpActorBuffer::ExternalActorEntry&
CdpActorBuffer::ExternalActorEntry::operator=(ExternalActorEntry&&) noexcept =
    default;
CdpActorBuffer::ExternalActorEntry::~ExternalActorEntry() = default;

CdpActorBuffer::CdpActorBuffer() = default;
CdpActorBuffer::~CdpActorBuffer() = default;

void CdpActorBuffer::OnExternalActorAttached(
    TransportType transport_type,
    std::optional<net::IPEndPoint> peer_endpoint,
    ProcessTree process_tree) {
  if (transport_type != chrome::cros::reporting::proto::ExternalProcessInfo::
                            TRANSPORT_TYPE_PIPE &&
      transport_type != chrome::cros::reporting::proto::ExternalProcessInfo::
                            TRANSPORT_TYPE_WEBSOCKET) {
    return;
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  PruneExpiredEntries(now);

  const base::ProcessId leaf_pid = GetLeafPid(process_tree);
  auto it = FindMatchingActor(transport_type, peer_endpoint, leaf_pid);
  if (it != external_actors_.end()) {
    ++it->active_sessions;
    it->detached_at.reset();
    if (peer_endpoint.has_value() && !it->peer_endpoint.has_value()) {
      it->peer_endpoint = std::move(peer_endpoint);
    }
    if (!process_tree.empty()) {
      it->leaf_pid = leaf_pid;
      it->process_tree = std::move(process_tree);
    }
    return;
  }

  const size_t max_actors = GetMaxActorsPerTab();
  while (external_actors_.size() >= max_actors) {
    EvictOneEntryForInsert();
  }

  external_actors_.emplace_back(transport_type, std::move(peer_endpoint),
                                std::move(process_tree));
}

void CdpActorBuffer::OnExternalActorAttached(TransportType transport_type,
                                             ProcessTree process_tree) {
  OnExternalActorAttached(transport_type, std::nullopt,
                          std::move(process_tree));
}

void CdpActorBuffer::OnExternalActorDetached(
    TransportType transport_type,
    std::optional<net::IPEndPoint> peer_endpoint,
    base::ProcessId pid) {
  const base::TimeTicks now = base::TimeTicks::Now();
  PruneExpiredEntries(now);

  auto it = FindMatchingActor(transport_type, peer_endpoint, pid);
  if (it == external_actors_.end() && !peer_endpoint.has_value() &&
      pid == base::kNullProcessId) {
    // Fallback for detaching an unattributed endpoint-less actor (e.g., a
    // WebSocket session whose remote endpoint was unavailable) without
    // conflating distinct unextracted actors during attachment.
    it = std::ranges::find_if(
        external_actors_, [transport_type](const ExternalActorEntry& entry) {
          return entry.transport_type == transport_type &&
                 entry.active_sessions > 0 &&
                 !entry.peer_endpoint.has_value() &&
                 entry.leaf_pid == base::kNullProcessId;
        });
  }
  if (it == external_actors_.end() || it->active_sessions == 0) {
    return;
  }

  --it->active_sessions;
  if (it->active_sessions == 0) {
    it->detached_at = now;
  }
}

void CdpActorBuffer::OnExternalActorDetached(TransportType transport_type,
                                             base::ProcessId pid) {
  OnExternalActorDetached(transport_type, std::nullopt, pid);
}

void CdpActorBuffer::UpdateExternalActorProcessTree(
    TransportType transport_type,
    std::optional<net::IPEndPoint> peer_endpoint,
    ProcessTree process_tree) {
  const base::TimeTicks now = base::TimeTicks::Now();
  PruneExpiredEntries(now);
  if (process_tree.empty()) {
    return;
  }

  const base::ProcessId leaf_pid = GetLeafPid(process_tree);
  auto it = FindMatchingActor(transport_type, peer_endpoint, leaf_pid);
  if (it == external_actors_.end()) {
    return;
  }

  it->leaf_pid = leaf_pid;
  it->process_tree = std::move(process_tree);
}

void CdpActorBuffer::OnDevToolsUiAttached() {
  PruneExpiredEntries(base::TimeTicks::Now());
  ++devtools_ui_active_sessions_;
}

void CdpActorBuffer::OnDevToolsUiDetached() {
  PruneExpiredEntries(base::TimeTicks::Now());
  if (devtools_ui_active_sessions_ > 0) {
    --devtools_ui_active_sessions_;
  }
}

std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
CdpActorBuffer::BuildEventInitiatorInfo() {
  PruneExpiredEntries(base::TimeTicks::Now());

  if (!external_actors_.empty()) {
    chrome::cros::reporting::proto::EventInitiatorInfo initiator;
    auto* external_process_info = initiator.mutable_external_process_info();
    for (const ExternalActorEntry& entry : external_actors_) {
      auto* external_process = external_process_info->add_external_processes();
      external_process->set_transport_type(entry.transport_type);
      for (const ProcessNode& node : entry.process_tree.process_nodes) {
        auto* proto_node = external_process->add_process_nodes();
        proto_node->set_process_id(node.process_id);
        proto_node->set_parent_process_id(node.parent_process_id);
        if (!node.executable_path.empty()) {
          proto_node->set_executable_path(node.executable_path);
        }
        if (!node.script_path.empty()) {
          proto_node->set_script_path(node.script_path);
        }
      }
    }
    return initiator;
  }

  if (devtools_ui_active_sessions_ > 0) {
    chrome::cros::reporting::proto::EventInitiatorInfo initiator;
    initiator.mutable_browser_feature_info()->set_feature_type(
        chrome::cros::reporting::proto::BrowserFeatureInfo::
            FEATURE_TYPE_DEVTOOLS_UI);
    return initiator;
  }

  return std::nullopt;
}

const std::vector<CdpActorBuffer::ExternalActorEntry>&
CdpActorBuffer::GetExternalActorsForTesting() {
  PruneExpiredEntries(base::TimeTicks::Now());
  return external_actors_;
}

void CdpActorBuffer::PruneExpiredEntries(base::TimeTicks now) {
  const base::TimeDelta ttl = GetDetachedActorTtl();

  std::erase_if(external_actors_, [&](const ExternalActorEntry& entry) {
    return !entry.IsAttached() && entry.detached_at.has_value() &&
           (now - *entry.detached_at >= ttl);
  });
}

std::vector<CdpActorBuffer::ExternalActorEntry>::iterator
CdpActorBuffer::FindMatchingActor(TransportType transport_type,
                                  std::optional<net::IPEndPoint> peer_endpoint,
                                  base::ProcessId leaf_pid) {
  if (peer_endpoint.has_value()) {
    for (auto it = external_actors_.begin(); it != external_actors_.end();
         ++it) {
      if (it->transport_type == transport_type &&
          it->peer_endpoint == peer_endpoint) {
        return it;
      }
    }
  }

  for (auto it = external_actors_.begin(); it != external_actors_.end(); ++it) {
    if (it->transport_type != transport_type) {
      continue;
    }

    if (peer_endpoint.has_value() && it->peer_endpoint.has_value()) {
      continue;
    }

    if (leaf_pid != base::kNullProcessId && it->leaf_pid == leaf_pid) {
      return it;
    }
    // `--remote-debugging-pipe` is a single browser-wide channel without a
    // socket endpoint, so an unresolved placeholder (`kNullProcessId`) and a
    // resolved Pipe entry (or a detach call omitting `pid`) refer to the same
    // pipe actor.
    if (!peer_endpoint.has_value() && !it->peer_endpoint.has_value() &&
        transport_type == chrome::cros::reporting::proto::ExternalProcessInfo::
                              TRANSPORT_TYPE_PIPE &&
        (leaf_pid == base::kNullProcessId ||
         it->leaf_pid == base::kNullProcessId)) {
      return it;
    }
  }
  return external_actors_.end();
}

void CdpActorBuffer::EvictOneEntryForInsert() {
  if (external_actors_.empty()) {
    return;
  }

  // Evict the oldest detached entry first; if all entries are still attached,
  // all map to `(true, std::nullopt)` so `std::ranges::min_element()` selects
  // `begin()` (the oldest attached entry).
  auto victim = std::ranges::min_element(
      external_actors_, {}, [](const ExternalActorEntry& entry) {
        return std::make_pair(entry.IsAttached(), entry.detached_at);
      });
  external_actors_.erase(victim);
}

TabCdpActorTracker::TabCdpActorTracker(content::WebContents* web_contents)
    : content::WebContentsUserData<TabCdpActorTracker>(*web_contents) {}

TabCdpActorTracker::~TabCdpActorTracker() = default;

// static
std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
TabCdpActorTracker::GetEventInitiatorInfo(content::WebContents* web_contents) {
  if (!web_contents ||
      !base::FeatureList::IsEnabled(kEventReportingCdpAttribution)) {
    return std::nullopt;
  }

  TabCdpActorTracker* tracker = FromWebContents(web_contents);
  if (!tracker) {
    return std::nullopt;
  }

  return tracker->buffer_.BuildEventInitiatorInfo();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabCdpActorTracker);

}  // namespace enterprise_connectors

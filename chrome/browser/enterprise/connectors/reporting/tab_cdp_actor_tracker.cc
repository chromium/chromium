// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/process/process_handle.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/process_extractor.h"
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

  const base::ProcessId leaf_pid = GetLeafPid(process_tree);
  auto it = FindMatchingActor(transport_type, peer_endpoint, leaf_pid);
  if (it != external_actors_.end()) {
    ++it->active_sessions;
    if (peer_endpoint.has_value() && !it->peer_endpoint.has_value()) {
      it->peer_endpoint = std::move(peer_endpoint);
    }
    if (!process_tree.empty()) {
      it->leaf_pid = leaf_pid;
      it->process_tree = std::move(process_tree);
    }
    return;
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
    external_actors_.erase(it);
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

std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
CdpActorBuffer::BuildEventInitiatorInfo() const {
  if (!base::FeatureList::IsEnabled(kEventReportingCdpAttribution)) {
    return std::nullopt;
  }

  if (external_actors_.empty()) {
    return std::nullopt;
  }

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

const std::vector<CdpActorBuffer::ExternalActorEntry>&
CdpActorBuffer::GetExternalActorsForTesting() const {
  return external_actors_;
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

}  // namespace enterprise_connectors

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/webrtc_event_provider.h"

#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"

WebRtcEventProvider::WebRtcEventProvider(UsageScenarioDataStoreImpl* data_store)
    : data_store_(data_store) {}

WebRtcEventProvider::~WebRtcEventProvider() {
  DCHECK(peer_connections_.empty());
  DCHECK(lids_per_renderers_.empty());
  DCHECK(!render_process_host_observations_.IsObservingAnySource());
}

void WebRtcEventProvider::OnPeerConnectionAdded(
    content::GlobalFrameRoutingId render_frame_host_id,
    int lid,
    base::ProcessId pid,
    const std::string& url,
    const std::string& rtc_configuration,
    const std::string& constraints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PeerConnectionId peer_connection_id(render_frame_host_id.child_id, lid);
  bool inserted = peer_connections_.emplace(peer_connection_id, false).second;
  DCHECK(inserted);

  MaybeAddRenderProcessHostObservation(render_frame_host_id.child_id, lid);
}

void WebRtcEventProvider::OnPeerConnectionRemoved(
    content::GlobalFrameRoutingId render_frame_host_id,
    int lid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PeerConnectionId peer_connection_id(render_frame_host_id.child_id, lid);
  auto it = peer_connections_.find(peer_connection_id);
  DCHECK(it != peer_connections_.end());
  const bool is_connected = it->second;

  if (is_connected)
    data_store_->OnWebRTCConnectionClosed();

  peer_connections_.erase(it);

  MaybeRemoveRenderProcessHostObservation(render_frame_host_id.child_id, lid);
}

void WebRtcEventProvider::OnPeerConnectionUpdated(
    content::GlobalFrameRoutingId render_frame_host_id,
    int lid,
    const std::string& type,
    const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PeerConnectionId peer_connection_id(render_frame_host_id.child_id, lid);
  auto it = peer_connections_.find(peer_connection_id);
  DCHECK(it != peer_connections_.end());
  bool& is_connected = it->second;

  const bool was_connected = is_connected;

  if (type == "iceConnectionStateChange") {
    if (value == "connected" || value == "checking" || value == "completed") {
      is_connected = true;
    } else if (value == "failed" || value == "disconnected" ||
               value == "closed" || value == "new") {
      is_connected = false;
    }
  } else if (type == "stop") {
    is_connected = false;
  }

  if (!was_connected && is_connected) {
    data_store_->OnWebRTCConnectionOpened();
  } else if (was_connected && !is_connected) {
    data_store_->OnWebRTCConnectionClosed();
  }
}

void WebRtcEventProvider::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int render_process_host_id = host->GetID();

  // Remove all peer connections that are associated with this render process.
  auto lower_bound_it = peer_connections_.lower_bound(PeerConnectionId(
      render_process_host_id, std::numeric_limits<int>::min()));
  auto upper_bound_it = peer_connections_.upper_bound(PeerConnectionId(
      render_process_host_id, std::numeric_limits<int>::max()));

  for (auto it = lower_bound_it; it != upper_bound_it; ++it) {
    const bool is_connected = it->second;
    if (is_connected)
      data_store_->OnWebRTCConnectionClosed();
  }
  peer_connections_.erase(lower_bound_it, upper_bound_it);

  // Stop tracking.
  size_t removed = lids_per_renderers_.erase(host);
  DCHECK_EQ(removed, 1u);
  render_process_host_observations_.RemoveObservation(host);
}

void WebRtcEventProvider::MaybeAddRenderProcessHostObservation(
    int render_process_host_id,
    int lid) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_host_id);
  DCHECK(render_process_host);

  auto insertion_result =
      lids_per_renderers_.emplace(render_process_host, base::flat_set<int>());
  base::flat_set<int>& lids = insertion_result.first->second;

  bool inserted = lids.insert(lid).second;
  DCHECK(inserted);

  // Add observation if this is the first peer connection that exists in this
  // RenderProcessHost
  if (insertion_result.second)
    render_process_host_observations_.AddObservation(render_process_host);
}

void WebRtcEventProvider::MaybeRemoveRenderProcessHostObservation(
    int render_process_host_id,
    int lid) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_host_id);
  DCHECK(render_process_host);

  auto it = lids_per_renderers_.find(render_process_host);
  DCHECK(it != lids_per_renderers_.end());
  base::flat_set<int>& lids = it->second;

  size_t removed = lids.erase(lid);
  DCHECK_EQ(removed, 1u);

  // Remove observation if there is no longer any peer connection that exists in
  // this RenderProcessHost.
  if (lids.empty()) {
    lids_per_renderers_.erase(it);
    render_process_host_observations_.RemoveObservation(render_process_host);
  }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_WEBRTC_EVENT_PROVIDER_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_WEBRTC_EVENT_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "content/public/browser/peer_connection_tracker_host_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

class UsageScenarioDataStoreImpl;

// Provides events related to WebRTC to the data store.
class WebRtcEventProvider : public content::PeerConnectionTrackerHostObserver,
                            public content::RenderProcessHostObserver {
 public:
  // This class will not own |data_store| so it needs to be outlived by this
  // provider.
  explicit WebRtcEventProvider(UsageScenarioDataStoreImpl* data_store);
  ~WebRtcEventProvider() override;

  WebRtcEventProvider(const WebRtcEventProvider& rhs) = delete;
  WebRtcEventProvider& operator=(const WebRtcEventProvider& rhs) = delete;

  // content::PeerConnectionTrackerHostObserver:
  void OnPeerConnectionAdded(content::GlobalFrameRoutingId render_frame_host_id,
                             int lid,
                             base::ProcessId pid,
                             const std::string& url,
                             const std::string& rtc_configuration,
                             const std::string& constraints) override;
  void OnPeerConnectionRemoved(
      content::GlobalFrameRoutingId render_frame_host_id,
      int lid) override;
  void OnPeerConnectionUpdated(
      content::GlobalFrameRoutingId render_frame_host_id,
      int lid,
      const std::string& type,
      const std::string& value) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

 private:
  // Adds/removes an observation to a RenderProcessHost if it is the first/last
  // peer connection to exist in that renderer.
  void MaybeAddRenderProcessHostObservation(int render_process_host_id,
                                            int lid);
  void MaybeRemoveRenderProcessHostObservation(int render_process_host_id,
                                               int lid);

  // The data store for the video capture events. Must outlive |this|.
  UsageScenarioDataStoreImpl* const data_store_;

  // For each existing peer connection, tracks whether it is actually connected
  // to another peer.
  using PeerConnectionId = std::pair<int, int>;
  base::flat_map<PeerConnectionId, bool /*is_connected*/> peer_connections_;

  // Tracks with which renderer each connection is associated with. Used to
  // ensure each RenderProcessHost is only observed once.
  base::flat_map<content::RenderProcessHost*, base::flat_set<int>>
      lids_per_renderers_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      render_process_host_observations_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_WEBRTC_EVENT_PROVIDER_H_

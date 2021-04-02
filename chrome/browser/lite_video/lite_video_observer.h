// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "chrome/common/lite_video_service.mojom.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace lite_video {
class LiteVideoDecider;
class LiteVideoHint;
}  // namespace lite_video

namespace optimization_guide {
enum class OptimizationGuideDecision;
}  // namespace optimization_guide

class LiteVideoObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LiteVideoObserver>,
      public lite_video::mojom::LiteVideoService {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~LiteVideoObserver() override;

  // Returns the total bytes estimated to be saved by LiteVideo.
  uint64_t GetAndClearEstimatedDataSavingBytes();

 private:
  friend class content::WebContentsUserData<LiteVideoObserver>;
  explicit LiteVideoObserver(content::WebContents* web_contents);

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaBufferUnderflow(const content::MediaPlayerId& id) override;
  void MediaPlayerSeek(const content::MediaPlayerId& id) override;

  // mojom::LiteVideoService.
  void NotifyThrottledDataUse(uint64_t response_bytes) override;

  // Determines the LiteVideoDecision based on |hint| and the coinflip
  // holdback state.
  lite_video::LiteVideoDecision MakeLiteVideoDecision(
      base::Optional<lite_video::LiteVideoHint> hint) const;

  // Records the metrics for LiteVideos applied to any frames associated with
  // the current mainframe navigation id. Called once per mainframe.
  void FlushUKMMetrics();

  // Updates the coinflip state if the navigation handle is associated with
  // the mainframe. Should only be called once per new mainframe navigation.
  void MaybeUpdateCoinflipExperimentState(
      content::NavigationHandle* navigation_handle);

  // Callback run after a hint and blocklist reason is available for use
  // within the agent associated with |render_frame_host_routing_id|.
  void OnHintAvailable(
      const content::GlobalFrameRoutingId& render_frame_host_routing_id,
      base::Optional<lite_video::LiteVideoHint> hint,
      lite_video::LiteVideoBlocklistReason blocklist_reason,
      optimization_guide::OptimizationGuideDecision opt_guide_decision);

  // Sends the |hint| to the render frame agent corresponding to the
  // provided global frame routing id.
  void SendHintToRenderFrameAgentForID(
      const content::GlobalFrameRoutingId& routing_id,
      const lite_video::LiteVideoHint& hint);

  // The decider capable of making decisions about whether LiteVideos should be
  // applied and the params to use when throttling media requests.
  lite_video::LiteVideoDecider* lite_video_decider_ = nullptr;

  // The current metrics about the navigation |this| is observing. Reset
  // after each time the metrics being held are recorded as a UKM event.
  base::Optional<lite_video::LiteVideoNavigationMetrics> nav_metrics_;

  // Whether the navigations currently being observed should have the LiteVideo
  // optimization heldback due to a coinflip, counterfactual experiment.
  // |is_coinflip_holdback_| is updated each time a mainframe navigation
  // commits.
  bool is_coinflip_holdback_ = false;

  // The set of routing ids corresponding to render frames that are waiting
  // for the decision of whether to throttle media requests that
  // occur within that frame.
  std::set<content::GlobalFrameRoutingId> routing_ids_to_notify_;

  // Current response bytes that have been targeted for LiteVideo throttling.
  uint64_t current_throttled_video_bytes_ = 0;

  content::WebContentsFrameReceiverSet<lite_video::mojom::LiteVideoService>
      receivers_;

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<LiteVideoObserver> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_

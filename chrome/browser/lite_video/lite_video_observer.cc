// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_observer.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "chrome/browser/lite_video/lite_video_decider.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_keyed_service.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "chrome/browser/lite_video/lite_video_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/previews_resource_loading_hints.mojom.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/proto/lite_video_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

namespace {

// Returns the LiteVideoDecider when the LiteVideo features is enabled.
lite_video::LiteVideoDecider* GetLiteVideoDeciderFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(lite_video::features::IsLiteVideoEnabled());
  if (!web_contents)
    return nullptr;

  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    return LiteVideoKeyedServiceFactory::GetForProfile(profile)
        ->lite_video_decider();
  }
  return nullptr;
}

}  // namespace

// static
void LiteVideoObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (IsLiteVideoAllowedForUser(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    LiteVideoObserver::CreateForWebContents(web_contents);
  }
}

LiteVideoObserver::LiteVideoObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      receivers_(web_contents, this) {
  lite_video_decider_ = GetLiteVideoDeciderFromWebContents(web_contents);
  routing_ids_to_notify_ = {};
}

LiteVideoObserver::~LiteVideoObserver() {
  FlushUKMMetrics();
}

void LiteVideoObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);

  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!lite_video_decider_)
    return;

  lite_video::LiteVideoBlocklistReason blocklist_reason =
      lite_video::LiteVideoBlocklistReason::kUnknown;

  if (navigation_handle->IsInMainFrame()) {
    FlushUKMMetrics();
    routing_ids_to_notify_.clear();
    nav_metrics_ = lite_video::LiteVideoNavigationMetrics(
        navigation_handle->GetNavigationId(),
        lite_video::LiteVideoDecision::kUnknown, blocklist_reason,
        lite_video::LiteVideoThrottleResult::kThrottledWithoutStop);
  }

  MaybeUpdateCoinflipExperimentState(navigation_handle);
  auto* render_frame_host = navigation_handle->GetRenderFrameHost();
  if (!render_frame_host)
    return;

  lite_video_decider_->CanApplyLiteVideo(
      navigation_handle,
      base::BindOnce(&LiteVideoObserver::OnHintAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     content::GlobalFrameRoutingId(
                         render_frame_host->GetProcess()->GetID(),
                         render_frame_host->GetRoutingID())));
}

void LiteVideoObserver::OnHintAvailable(
    const content::GlobalFrameRoutingId& render_frame_host_routing_id,
    base::Optional<lite_video::LiteVideoHint> hint,
    lite_video::LiteVideoBlocklistReason blocklist_reason,
    optimization_guide::OptimizationGuideDecision opt_guide_decision) {
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_routing_id);
  if (!render_frame_host)
    return;

  bool is_mainframe = render_frame_host->GetMainFrame() == render_frame_host;
  if (!is_mainframe &&
      opt_guide_decision ==
          optimization_guide::OptimizationGuideDecision::kUnknown) {
    // If this is a subframe and the decision was unknown, then the decision
    // from the optimization guide (queried only for the mainframe) has not
    // returned.
    //
    // TODO(crbug/1121833): Add histogram to capture the size of the set of
    // routing ids.
    routing_ids_to_notify_.insert(render_frame_host_routing_id);
    return;
  }
  lite_video::LiteVideoDecision decision = MakeLiteVideoDecision(hint);

  LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.Navigation.HasHint", hint ? true : false);
  // TODO(crbug/1117064): Add a global blocklist check to ensure that the host
  // commited of the committed URL is allowed to have LiteVideos applied.
  if (nav_metrics_ && is_mainframe) {
    nav_metrics_->SetDecision(decision);
    nav_metrics_->SetBlocklistReason(blocklist_reason);
  }

  // Only proceed to passing hints if the decision is allowed.
  if (decision != lite_video::LiteVideoDecision::kAllowed &&
      opt_guide_decision !=
          optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }

  if (!hint)
    return;

  if (!render_frame_host || !render_frame_host->GetProcess())
    return;

  // At this stage, the following criteria for the render frame should be true:
  // 1. The render frame for the routing id is valid.
  // 2. The current navigation within the frame is not blocked.
  // 3. The decision to apply a LiteVideo is true.
  // 4. A LiteVideo hint is available.
  SendHintToRenderFrameAgentForID(render_frame_host_routing_id, *hint);

  if (is_mainframe) {
    // Given that this is the mainframe, we need to notify all the subframes
    // that have requested LiteVideo hints but the optimization guide had
    // not returned a decision.
    for (const auto& routing_id : routing_ids_to_notify_)
      SendHintToRenderFrameAgentForID(routing_id, *hint);
    routing_ids_to_notify_.clear();
    return;
  }
}

void LiteVideoObserver::SendHintToRenderFrameAgentForID(
    const content::GlobalFrameRoutingId& routing_id,
    const lite_video::LiteVideoHint& hint) {
  auto* render_frame_host = content::RenderFrameHost::FromID(routing_id);
  if (!render_frame_host)
    return;

  mojo::AssociatedRemote<previews::mojom::PreviewsResourceLoadingHintsReceiver>
      loading_hints_agent;

  auto hint_ptr = previews::mojom::LiteVideoHint::New();
  hint_ptr->target_downlink_bandwidth_kbps =
      hint.target_downlink_bandwidth_kbps();
  hint_ptr->kilobytes_to_buffer_before_throttle =
      hint.kilobytes_to_buffer_before_throttle();
  hint_ptr->target_downlink_rtt_latency = hint.target_downlink_rtt_latency();
  hint_ptr->max_throttling_delay = hint.max_throttling_delay();

  if (render_frame_host->GetRemoteAssociatedInterfaces()) {
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &loading_hints_agent);
    loading_hints_agent->SetLiteVideoHint(std::move(hint_ptr));
  }
}

lite_video::LiteVideoDecision LiteVideoObserver::MakeLiteVideoDecision(
    base::Optional<lite_video::LiteVideoHint> hint) const {
  if (hint) {
    return is_coinflip_holdback_ ? lite_video::LiteVideoDecision::kHoldback
                                 : lite_video::LiteVideoDecision::kAllowed;
  }
  return lite_video::LiteVideoDecision::kNotAllowed;
}

void LiteVideoObserver::FlushUKMMetrics() {
  if (!nav_metrics_)
    return;
  ukm::SourceId ukm_source_id = ukm::ConvertToSourceId(
      nav_metrics_->nav_id(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::LiteVideo builder(ukm_source_id);
  builder.SetThrottlingStartDecision(static_cast<int>(nav_metrics_->decision()))
      .SetBlocklistReason(static_cast<int>(nav_metrics_->blocklist_reason()))
      .SetThrottlingResult(static_cast<int>(nav_metrics_->throttle_result()))
      .Record(ukm::UkmRecorder::Get());
  nav_metrics_.reset();
}

// Returns the result of a coinflip.
void LiteVideoObserver::MaybeUpdateCoinflipExperimentState(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  if (!lite_video::features::IsCoinflipExperimentEnabled())
    return;

  is_coinflip_holdback_ = lite_video::switches::ShouldForceCoinflipHoldback()
                              ? true
                              : base::RandInt(0, 1);
}

void LiteVideoObserver::MediaBufferUnderflow(const content::MediaPlayerId& id) {
  auto* render_frame_host =
      content::RenderFrameHost::FromID(id.frame_routing_id);

  if (!render_frame_host || !render_frame_host->GetProcess())
    return;

  // Only consider a rebuffer event related to LiteVideos if they
  // were allowed on current navigation.
  if (!nav_metrics_ ||
      nav_metrics_->decision() != lite_video::LiteVideoDecision::kAllowed) {
    return;
  }

  mojo::AssociatedRemote<previews::mojom::PreviewsResourceLoadingHintsReceiver>
      loading_hints_agent;

  if (!render_frame_host->GetRemoteAssociatedInterfaces())
    return;

  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &loading_hints_agent);

  if (nav_metrics_->ShouldStopOnRebufferForFrame(
          render_frame_host->GetRoutingID())) {
    loading_hints_agent->StopThrottlingMediaRequests();
  }

  if (!lite_video_decider_)
    return;

  // Determine and log if the rebuffer happened in the mainframe.
  render_frame_host->GetMainFrame() == render_frame_host
      ? lite_video_decider_->DidMediaRebuffer(
            render_frame_host->GetLastCommittedURL(), base::nullopt, true)
      : lite_video_decider_->DidMediaRebuffer(
            render_frame_host->GetMainFrame()->GetLastCommittedURL(),
            render_frame_host->GetLastCommittedURL(), true);
}

void LiteVideoObserver::MediaPlayerSeek(const content::MediaPlayerId& id) {

  if (!lite_video::features::DisableLiteVideoOnMediaPlayerSeek())
    return;

  auto* render_frame_host =
      content::RenderFrameHost::FromID(id.frame_routing_id);
  if (!render_frame_host || !render_frame_host->GetProcess())
    return;

  // Only consider a seek event related to LiteVideos if they were allowed on
  // current navigation.
  if (!nav_metrics_ ||
      nav_metrics_->decision() != lite_video::LiteVideoDecision::kAllowed) {
    return;
  }

  mojo::AssociatedRemote<previews::mojom::PreviewsResourceLoadingHintsReceiver>
      loading_hints_agent;

  if (!render_frame_host->GetRemoteAssociatedInterfaces())
    return;

  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &loading_hints_agent);

  LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.MediaPlayerSeek.StopThrottling", true);

  loading_hints_agent->StopThrottlingMediaRequests();
}

uint64_t LiteVideoObserver::GetAndClearEstimatedDataSavingBytes() {
  if (current_throttled_video_bytes_ == 0)
    return 0;
  // ThrottledVideoBytesDeflatedRatio is essentially
  //  bytes_saved / throttled_video_bytes. So use that to compute estimated
  //  bytes saved.
  uint64_t bytes_saved = static_cast<uint64_t>(
      current_throttled_video_bytes_ *
      lite_video::features::GetThrottledVideoBytesDeflatedRatio());
  current_throttled_video_bytes_ = 0;
  return bytes_saved;
}

void LiteVideoObserver::NotifyThrottledDataUse(uint64_t response_bytes) {
  current_throttled_video_bytes_ += response_bytes;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LiteVideoObserver)

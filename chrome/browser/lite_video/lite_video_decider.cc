// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_decider.h"

#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_hint_cache.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "chrome/browser/lite_video/lite_video_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/proto/lite_video_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/nqe/effective_connection_type.h"
#include "ui/base/page_transition_types.h"

namespace {

// Utility class for recording the decision of whether LiteVideos should be
// applied to a navigation and if a LiteVideoHint is available for the
// navigation. The result is recorded when it goes out of scope and its
// destructor is called.
class ScopedLiteVideoDecisionRecorder {
 public:
  explicit ScopedLiteVideoDecisionRecorder(
      lite_video::LiteVideoBlocklistReason blocklist_reason,
      bool is_mainframe)
      : blocklist_reason_(blocklist_reason),
        is_mainframe_(is_mainframe),
        has_hint_for_host_(false) {}
  ~ScopedLiteVideoDecisionRecorder() {
    if (is_mainframe_) {
      UMA_HISTOGRAM_ENUMERATION(
          "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
          blocklist_reason_);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame",
          blocklist_reason_);
    }
    UMA_HISTOGRAM_BOOLEAN("LiteVideo.CanApplyLiteVideo.HintCache.HasHint",
                          has_hint_for_host_);
  }
  void set_has_hint_for_host(bool has_hint_for_host) {
    has_hint_for_host_ = has_hint_for_host;
  }

 private:
  lite_video::LiteVideoBlocklistReason blocklist_reason_;
  bool is_mainframe_;
  bool has_hint_for_host_;
};

bool CanApplyOnCurrentNetworkConditions(
    bool is_cellular_network,
    net::EffectiveConnectionType effective_connection_type) {
  if (lite_video::switches::ShouldIgnoreLiteVideoNetworkConditions())
    return true;

  if (!is_cellular_network)
    return false;

  return effective_connection_type >= lite_video::features::MinLiteVideoECT();
}

}  // namespace

namespace lite_video {

LiteVideoDecider::LiteVideoDecider(
    std::unique_ptr<blocklist::OptOutStore> opt_out_store,
    base::Clock* clock,
    optimization_guide::OptimizationGuideDecider* opt_guide_decider)
    : hint_cache_(std::make_unique<LiteVideoHintCache>()),
      opt_guide_decider_(opt_guide_decider),
      cached_opt_guide_hints_(features::MaxOptimizationGuideHintCacheSize()),
      permanent_host_blocklist_(features::GetLiteVideoPermanentBlocklist()) {
  user_blocklist_ = std::make_unique<LiteVideoUserBlocklist>(
      std::move(opt_out_store), clock, this);

  if (opt_guide_decider_) {
    opt_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_VIDEO});
  }

  network::NetworkQualityTracker* nqe_tracker =
      g_browser_process->network_quality_tracker();
  if (nqe_tracker) {
    nqe_tracker->AddEffectiveConnectionTypeObserver(this);
    current_effective_connection_type_ =
        nqe_tracker->GetEffectiveConnectionType();
  }

  network::NetworkConnectionTracker* network_connection_tracker =
      content::GetNetworkConnectionTracker();
  if (network_connection_tracker) {
    network_connection_tracker->AddNetworkConnectionObserver(this);
    network::mojom::ConnectionType connection_type =
        network::mojom::ConnectionType::CONNECTION_UNKNOWN;
    network_connection_tracker->GetConnectionType(&connection_type,
                                                  base::DoNothing());
    is_cellular_network_ =
        network_connection_tracker->IsConnectionCellular(connection_type);
  }
}

LiteVideoDecider::~LiteVideoDecider() {
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void LiteVideoDecider::CanApplyLiteVideo(
    content::NavigationHandle* navigation_handle,
    LiteVideoHintCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LiteVideoBlocklistReason blocklist_reason =
      LiteVideoBlocklistReason::kUnknown;
  if (!IsLiteVideoAllowedForUser(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext()))) {
    std::move(callback).Run(
        base::nullopt, blocklist_reason,
        optimization_guide::OptimizationGuideDecision::kFalse);
    return;
  }

  if (switches::ShouldOverrideLiteVideoDecision()) {
    // Return a default configured hint.
    std::move(callback).Run(
        LiteVideoHint(switches::GetDefaultDownlinkBandwidthKbps(),
                      features::LiteVideoTargetDownlinkRTTLatency(),
                      features::LiteVideoKilobytesToBufferBeforeThrottle(),
                      features::LiteVideoMaxThrottlingDelay()),
        blocklist_reason, optimization_guide::OptimizationGuideDecision::kTrue);
    return;
  }

  if (!CanApplyOnCurrentNetworkConditions(is_cellular_network_,
                                          current_effective_connection_type_)) {
    std::move(callback).Run(
        base::nullopt, blocklist_reason,
        optimization_guide::OptimizationGuideDecision::kFalse);
    return;
  }

  GURL url = navigation_handle->GetURL();

  if (!url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(
        base::nullopt, blocklist_reason,
        optimization_guide::OptimizationGuideDecision::kFalse);
    return;
  }

  if (url.has_host() && IsHostPermanentlyBlockedlisted(url.host())) {
    blocklist_reason = LiteVideoBlocklistReason::kHostPermanentlyBlocklisted;
    ScopedLiteVideoDecisionRecorder scoped_decision_recorder(
        blocklist_reason, navigation_handle->IsInMainFrame());
    std::move(callback).Run(
        base::nullopt, blocklist_reason,
        optimization_guide::OptimizationGuideDecision::kFalse);
    return;
  }

  // Reloads and Forward-Back navigations are considered opt-outs and are added
  // to the blocklist so that a host that is frequently reloaded on does not get
  // LiteVideos.
  bool is_reload = PageTransitionCoreTypeIs(
      navigation_handle->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
  if (is_reload || features::IsLiteVideoNotAllowedForPageTransition(
                       navigation_handle->GetPageTransition())) {
    user_blocklist_->AddNavigationToBlocklist(navigation_handle, true);
    blocklist_reason = is_reload
                           ? LiteVideoBlocklistReason::kNavigationReload
                           : LiteVideoBlocklistReason::kNavigationForwardBack;
    ScopedLiteVideoDecisionRecorder scoped_decision_recorder(
        blocklist_reason, navigation_handle->IsInMainFrame());
    std::move(callback).Run(
        base::nullopt, blocklist_reason,
        optimization_guide::OptimizationGuideDecision::kFalse);
    return;
  }

  blocklist_reason =
      user_blocklist_->IsLiteVideoAllowedOnNavigation(navigation_handle);

  if (opt_guide_decider_) {
    // This relies on the optimization guide for hints.
    if (navigation_handle->IsInMainFrame()) {
      opt_guide_decider_->CanApplyOptimizationAsync(
          navigation_handle, optimization_guide::proto::LITE_VIDEO,
          base::BindOnce(&LiteVideoDecider::OnOptimizationGuideHintAvailable,
                         weak_ptr_factory_.GetWeakPtr(),
                         navigation_handle->GetURL(), blocklist_reason,
                         std::move(callback)));

      UpdateBlocklists(navigation_handle, blocklist_reason);
      return;
    }

    // For subframes, check if a hint is cached that can be used
    // immediately. Otherwise, the callback from the optimization guide
    // will trigger subframes to get the supplied hint.
    base::Optional<LiteVideoHint> hint;
    optimization_guide::OptimizationGuideDecision opt_guide_decision =
        optimization_guide::OptimizationGuideDecision::kUnknown;
    GURL mainframe_url =
        navigation_handle->GetWebContents()->GetLastCommittedURL();
    auto it = cached_opt_guide_hints_.Get(mainframe_url.host());
    if (it != cached_opt_guide_hints_.end()) {
      hint = it->second;
      // An entry with an empty hint means that the optimization guide
      // decision was kFalse.
      opt_guide_decision =
          hint ? optimization_guide::OptimizationGuideDecision::kTrue
               : optimization_guide::OptimizationGuideDecision::kFalse;
    }

    UpdateBlocklists(navigation_handle, blocklist_reason);

    ScopedLiteVideoDecisionRecorder scoped_decision_recorder(
        blocklist_reason, navigation_handle->IsInMainFrame());
    if (hint)
      scoped_decision_recorder.set_has_hint_for_host(true);

    std::move(callback).Run(hint, blocklist_reason, opt_guide_decision);
    return;
  }

  base::Optional<LiteVideoHint> hint =
      hint_cache_->GetHintForNavigationURL(url);
  ScopedLiteVideoDecisionRecorder scoped_decision_recorder(
      blocklist_reason, navigation_handle->IsInMainFrame());

  if (hint)
    scoped_decision_recorder.set_has_hint_for_host(true);

  if (blocklist_reason != LiteVideoBlocklistReason::kAllowed || !hint) {
    std::move(callback).Run(
        base::nullopt, blocklist_reason,
        optimization_guide::OptimizationGuideDecision::kFalse);
    return;
  }

  UpdateBlocklists(navigation_handle, blocklist_reason);
  std::move(callback).Run(hint, blocklist_reason,
                          optimization_guide::OptimizationGuideDecision::kTrue);
}

void LiteVideoDecider::UpdateBlocklists(
    content::NavigationHandle* navigation_handle,
    LiteVideoBlocklistReason blocklist_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);
  if (blocklist_reason != LiteVideoBlocklistReason::kAllowed)
    return;

  // The navigation was not blocklisted and may
  // have the LiteVideo optimization triggered so update the blocklist.
  user_blocklist_->AddNavigationToBlocklist(navigation_handle, false);

  navigation_handle->IsInMainFrame()
      ? DidMediaRebuffer(navigation_handle->GetURL(), base::nullopt, false)
      : DidMediaRebuffer(
            navigation_handle->GetWebContents()->GetLastCommittedURL(),
            navigation_handle->GetURL(), false);
}

void LiteVideoDecider::OnOptimizationGuideHintAvailable(
    const GURL& mainframe_url,
    LiteVideoBlocklistReason blocklist_reason,
    LiteVideoHintCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opt_guide_decider_);

  // This is only called on a mainframe navigation.
  ScopedLiteVideoDecisionRecorder scoped_decision_recorder(
      blocklist_reason, /*is_mainframe=*/true);

  if (decision == optimization_guide::OptimizationGuideDecision::kTrue)
    scoped_decision_recorder.set_has_hint_for_host(true);

  // If the decision is false, then add an empty entry into the hint cache
  // so that subframes with this mainframe host will return false.
  if (decision == optimization_guide::OptimizationGuideDecision::kFalse) {
    cached_opt_guide_hints_.Put(mainframe_url.host(), base::nullopt);
    UMA_HISTOGRAM_COUNTS_100("LiteVideo.LiteVideoDecider.OptGuideHintCacheSize",
                             cached_opt_guide_hints_.size());
  }

  if (blocklist_reason != LiteVideoBlocklistReason::kAllowed ||
      decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(base::nullopt, blocklist_reason, decision);
    return;
  }

  LiteVideoHint hint =
      LiteVideoHint(switches::GetDefaultDownlinkBandwidthKbps(),
                    features::LiteVideoTargetDownlinkRTTLatency(),
                    features::LiteVideoKilobytesToBufferBeforeThrottle(),
                    features::LiteVideoMaxThrottlingDelay());

  base::Optional<optimization_guide::proto::LiteVideoMetadata>
      lite_video_metadata =
          metadata
              .ParsedMetadata<optimization_guide::proto::LiteVideoMetadata>();
  if (lite_video_metadata && lite_video_metadata->has_lite_video_hint())
    hint = LiteVideoHint(lite_video_metadata->lite_video_hint());

  cached_opt_guide_hints_.Put(mainframe_url.host(), hint);
  UMA_HISTOGRAM_COUNTS_100("LiteVideo.LiteVideoDecider.OptGuideHintCacheSize",
                           cached_opt_guide_hints_.size());
  std::move(callback).Run(hint, blocklist_reason, decision);
}

void LiteVideoDecider::OnLoadingStateChanged(bool is_loaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blocklist_loaded_ = is_loaded;
  if (blocklist_loaded_)
    LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.UserBlocklist.BlocklistLoaded", true);
}

void LiteVideoDecider::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_effective_connection_type_ = effective_connection_type;
}

void LiteVideoDecider::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_cellular_network_ =
      network::NetworkConnectionTracker::IsConnectionCellular(type);
}

void LiteVideoDecider::ClearData(const base::Time& delete_begin,
                                 const base::Time& delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (user_blocklist_)
    user_blocklist_->ClearBlockList(delete_begin, delete_end);
  cached_opt_guide_hints_.Clear();
}

void LiteVideoDecider::OnBlocklistCleared(base::Time time) {
  LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.UserBlocklist.ClearBlocklist", true);
}

bool LiteVideoDecider::IsHostPermanentlyBlockedlisted(
    const std::string& host) const {
  if (permanent_host_blocklist_.size() == 0)
    return false;
  return permanent_host_blocklist_.find(host) !=
         permanent_host_blocklist_.end();
}

void LiteVideoDecider::DidMediaRebuffer(const GURL& mainframe_url,
                                        base::Optional<GURL> subframe_url,
                                        bool opt_out) {
  if (user_blocklist_) {
    user_blocklist_->AddRebufferToBlocklist(mainframe_url, subframe_url,
                                            opt_out);
  }
}

}  // namespace lite_video

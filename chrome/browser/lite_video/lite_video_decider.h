// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_DECIDER_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_DECIDER_H_

#include <stdint.h>

#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "chrome/browser/lite_video/lite_video_hint_cache.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/gurl.h"

namespace blocklist {
class OptOutStore;
}  // namespace blocklist

namespace optimization_guide {
class OptimizationGuideDecider;
enum class OptimizationGuideDecision;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace lite_video {

using LiteVideoHintCallback = base::OnceCallback<void(
    base::Optional<LiteVideoHint> hint,
    LiteVideoBlocklistReason blocklist_reason,
    optimization_guide::OptimizationGuideDecision opt_guide_decision)>;

// The LiteVideoDecider makes the decision on whether LiteVideos should be
// applied to a navigation and provides the parameters to use when
// throttling media requests.
class LiteVideoDecider
    : public blocklist::OptOutBlocklistDelegate,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  LiteVideoDecider(
      std::unique_ptr<blocklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      optimization_guide::OptimizationGuideDecider* opt_guide_decider);
  ~LiteVideoDecider() override;

  // Determine if the navigation can have the LiteVideo optimization applied. It
  // invokes |callback| with the blocklist result and a LiteVideoHint if
  // available for the |navigation_handle|. This also updates the blocklist
  // based on the navigation provided and should be limited to one call per
  // navigation.
  void CanApplyLiteVideo(content::NavigationHandle* navigation_handle,
                         LiteVideoHintCallback callback);

  // Override the blocklist used by |this| for testing.
  void SetUserBlocklistForTesting(
      std::unique_ptr<LiteVideoUserBlocklist> user_blocklist) {
    user_blocklist_ = std::move(user_blocklist);
  }

  // Override the hint cache used by |this| for testing.
  void SetHintCacheForTesting(std::unique_ptr<LiteVideoHintCache> hint_cache) {
    hint_cache_ = std::move(hint_cache);
  }

  // blocklist::OptOutBlocklistDelegate
  void OnLoadingStateChanged(bool is_loaded) override;
  void OnBlocklistCleared(base::Time time) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  // Purge all the user browsing data within |user_blocklist_|  between
  // the provided time ranges and all data within |cached_opt_guide_hints_|.
  void ClearData(const base::Time& delete_begin, const base::Time& delete_end);

  // Update |user_blocklist_| that a rebuffer event consided an opt-out on the
  // mainframe and subframe URLs occurred.
  void DidMediaRebuffer(const GURL& mainframe_url,
                        base::Optional<GURL> subframe_url,
                        bool opt_out);

  // Set the optimization guide decider used by |this| for testing only.
  void SetOptimizationGuideDeciderForTesting(
      optimization_guide::OptimizationGuideDecider* opt_guide_decider) {
    opt_guide_decider_ = opt_guide_decider;
  }

  // Set the permanently blocked hosts used by |this| for testing only.
  void SetPermanentHostBlocklistForTesting(
      const base::flat_set<std::string>& permanent_host_blocklist) {
    permanent_host_blocklist_ = permanent_host_blocklist;
  }

 private:
  // The result of the query to the optimization guide based on the
  // |mainframe_url|.
  void OnOptimizationGuideHintAvailable(
      const GURL& mainframe_url,
      LiteVideoBlocklistReason blocklist_reason,
      LiteVideoHintCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Update the blocklists based on the navigation and the provided blocklist
  // reason.
  void UpdateBlocklists(content::NavigationHandle* navigation_handle,
                        LiteVideoBlocklistReason blocklist_reason);

  // Checks the owned permanent blocklist to determine if |host|
  // has been blocked from having LiteVideos shown indefinitely.
  bool IsHostPermanentlyBlockedlisted(const std::string& host) const;

  // The hint cache that holds LiteVideoHints that specify the parameters
  // for throttling media requests for that navigation.
  std::unique_ptr<LiteVideoHintCache> hint_cache_;

  // The blocklist that maintains the hosts that should not have media requests
  // throttled on them due to too many opt-outs.
  std::unique_ptr<LiteVideoUserBlocklist> user_blocklist_;

  // Whether the backing store used by the owned |user_blocklist_| is loaded
  // and available.
  bool blocklist_loaded_ = false;

  // Whether the current network connection is cellular or not.
  bool is_cellular_network_ = false;

  // The current estimate of the EffectiveConnectionType.
  net::EffectiveConnectionType current_effective_connection_type_ =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  // The optimization guide decider to consult for remote predictions.
  optimization_guide::OptimizationGuideDecider* opt_guide_decider_ = nullptr;

  // The store of hints provided by the optimization guide keyed by mainframe
  // host. If the hint is empty, then the optimization guide returned kFalse.
  base::HashingMRUCache<std::string, base::Optional<LiteVideoHint>>
      cached_opt_guide_hints_;

  // The set of hosts that are permanently blocked from having LiteVideos
  // applied on them.
  base::flat_set<std::string> permanent_host_blocklist_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<LiteVideoDecider> weak_ptr_factory_{this};
};

}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_DECIDER_H_

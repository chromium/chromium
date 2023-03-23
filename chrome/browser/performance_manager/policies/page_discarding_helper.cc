// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {
namespace {

// Time during which a tab cannot be discarded after having played audio.
constexpr base::TimeDelta kTabAudioProtectionTime = base::Minutes(1);

// NodeAttachedData used to indicate that there's already been an attempt to
// discard a PageNode.
// TODO(sebmarchand): The only reason for a discard attempt to fail is if we try
// to discard a prerenderer, remove this once we can detect if a PageNode is a
// prerenderer in CanDiscard().
class DiscardAttemptMarker : public NodeAttachedDataImpl<DiscardAttemptMarker> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};
  ~DiscardAttemptMarker() override = default;

 private:
  friend class ::performance_manager::NodeAttachedDataImpl<
      DiscardAttemptMarker>;
  explicit DiscardAttemptMarker(const PageNodeImpl* page_node) {}
};

const char kDescriberName[] = "PageDiscardingHelper";

using NodeRssMap = base::flat_map<const PageNode*, uint64_t>;

// Returns the mapping from page_node to its RSS estimation.
NodeRssMap GetPageNodeRssEstimateKb(
    const std::vector<PageNodeSortProxy>& candidates) {
  // Initialize the result map in one shot for time complexity O(n * log(n)).
  NodeRssMap::container_type result_container;
  result_container.reserve(candidates.size());
  for (auto candidate : candidates) {
    result_container.emplace_back(candidate.page_node(), 0);
  }
  NodeRssMap result(std::move(result_container));

  // TODO(crbug/1240994): Use visitor to accumulate the result to avoid
  // allocating extra lists of frame nodes behind the scenes.

  // List all the processes associated with these page nodes.
  base::flat_set<const ProcessNode*> process_nodes;
  for (auto candidate : candidates) {
    base::flat_set<const ProcessNode*> processes =
        GraphOperations::GetAssociatedProcessNodes(candidate.page_node());
    process_nodes.insert(processes.begin(), processes.end());
  }

  // Compute the resident set of each page by simply summing up the estimated
  // resident set of all its frames.
  for (const ProcessNode* process_node : process_nodes) {
    base::flat_set<const FrameNode*> process_frames =
        process_node->GetFrameNodes();
    if (!process_frames.size()) {
      continue;
    }
    // Get the resident set of the process and split it equally across its
    // frames.
    const uint64_t frame_rss_kb =
        process_node->GetResidentSetKb() / process_frames.size();
    for (const FrameNode* frame_node : process_frames) {
      // Check if the frame belongs to a discardable page, if so update the
      // resident set of the page.
      auto iter = result.find(frame_node->GetPageNode());
      if (iter == result.end()) {
        continue;
      }
      iter->second += frame_rss_kb;
    }
  }
  return result;
}

}  // namespace

PageDiscardingHelper::PageDiscardingHelper()
    : page_discarder_(std::make_unique<mechanism::PageDiscarder>()) {}
PageDiscardingHelper::~PageDiscardingHelper() = default;

void PageDiscardingHelper::DiscardAPage(
    base::OnceCallback<void(bool)> post_discard_cb,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  DiscardMultiplePages(absl::nullopt, false, std::move(post_discard_cb),
                       discard_reason, minimum_time_in_background);
}

void PageDiscardingHelper::DiscardMultiplePages(
    absl::optional<uint64_t> reclaim_target_kb,
    bool discard_protected_tabs,
    base::OnceCallback<void(bool)> post_discard_cb,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(WARNING) << "Discarding multiple pages with target (kb): "
               << (reclaim_target_kb ? *reclaim_target_kb : 0);

  // Ensures running post_discard_cb on early return.
  auto split_callback = base::SplitOnceCallback(std::move(post_discard_cb));
  base::ScopedClosureRunner run_post_discard_cb_on_return(
      base::BindOnce(std::move(split_callback.first), false));

  std::vector<const PageNode*> page_nodes = graph_->GetAllPageNodes();

  std::vector<PageNodeSortProxy> candidates;
  for (const auto* page_node : page_nodes) {
    CanDiscardResult can_discard_result =
        CanDiscard(page_node, discard_reason, minimum_time_in_background);
    if (can_discard_result == CanDiscardResult::kMarked) {
      continue;
    }
    bool is_protected = (can_discard_result == CanDiscardResult::kProtected);
    if (!discard_protected_tabs && is_protected) {
      continue;
    }
    candidates.emplace_back(page_node, false, is_protected,
                            page_node->GetTimeSinceLastVisibilityChange());
  }
  // Sorts with ascending importance.
  std::sort(candidates.begin(), candidates.end());

  UMA_HISTOGRAM_COUNTS_100("Discarding.DiscardCandidatesCount",
                           candidates.size());

  // Returns early when candidate is empty to avoid infinite loop in
  // DiscardMultiplePages and PostDiscardAttemptCallback.
  if (candidates.empty()) {
    return;
  }
  std::vector<const PageNode*> discard_attempts;

  if (reclaim_target_kb == absl::nullopt) {
    const PageNode* oldest = candidates[0].page_node();
    discard_attempts.emplace_back(oldest);
  } else {
    const uint64_t reclaim_target_kb_value = *reclaim_target_kb;
    uint64_t total_reclaim_kb = 0;
    NodeRssMap page_node_rss_kb = GetPageNodeRssEstimateKb(candidates);
    for (auto& candidate : candidates) {
      if (total_reclaim_kb >= reclaim_target_kb_value) {
        break;
      }
      const PageNode* node = candidate.page_node();
      discard_attempts.emplace_back(node);
      // The node RSS value is updated by ProcessMetricsDecorator periodically.
      // The RSS value is 0 for nodes that have never been updated, estimate the
      // RSS value to 80 MiB for these nodes. 80 MiB is the average
      // Memory.Renderer.PrivateMemoryFootprint histogram value on Windows in
      // August 2021.
      total_reclaim_kb +=
          (page_node_rss_kb[node]) ? page_node_rss_kb[node] : 80 * 1024;
    }
  }

  if (discard_attempts.empty()) {
    return;
  }

  // Adorns the PageNodes with a discard attempt marker to make sure that we
  // don't try to discard it multiple times if it fails to be discarded. In
  // practice this should only happen to prerenderers.
  for (auto* attempt : discard_attempts) {
    DiscardAttemptMarker::GetOrCreate(PageNodeImpl::FromNode(attempt));
  }

  // Got to the end successfully, don't call the early return callback.
  run_post_discard_cb_on_return.ReplaceClosure(base::DoNothing());

  LOG(WARNING) << "Discarding " << discard_attempts.size() << " pages";

  page_discarder_->DiscardPageNodes(
      discard_attempts, discard_reason,
      base::BindOnce(&PageDiscardingHelper::PostDiscardAttemptCallback,
                     weak_factory_.GetWeakPtr(), reclaim_target_kb,
                     discard_protected_tabs, std::move(split_callback.second),
                     discard_reason, minimum_time_in_background));
}

void PageDiscardingHelper::ImmediatelyDiscardSpecificPage(
    const PageNode* page_node,
    DiscardReason discard_reason,
    base::OnceCallback<void(bool)> post_discard_cb) {
  // Pass 0 TimeDelta to bypass the minimum time in background check.
  if (CanDiscard(page_node, discard_reason,
                 /*minimum_time_in_background=*/base::TimeDelta()) ==
      CanDiscardResult::kEligible) {
    page_discarder_->DiscardPageNodes({page_node}, discard_reason,
                                      std::move(post_discard_cb));
  } else {
    std::move(post_discard_cb).Run(false);
  }
}

void PageDiscardingHelper::OnBeforePageNodeRemoved(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_change_to_non_audible_time_.erase(page_node);
}

void PageDiscardingHelper::OnIsAudibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!page_node->IsAudible()) {
    last_change_to_non_audible_time_[page_node] = base::TimeTicks::Now();
  }
}

void PageDiscardingHelper::SetNoDiscardPatternsForProfile(
    const std::string& browser_context_id,
    const std::vector<std::string>& patterns) {
  std::unique_ptr<url_matcher::URLMatcher>& entry =
      profiles_no_discard_patterns_[browser_context_id];
  entry = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddAllowFilters(entry.get(), patterns);
}

void PageDiscardingHelper::ClearNoDiscardPatternsForProfile(
    const std::string& browser_context_id) {
  profiles_no_discard_patterns_.erase(browser_context_id);
}

void PageDiscardingHelper::SetMockDiscarderForTesting(
    std::unique_ptr<mechanism::PageDiscarder> discarder) {
  page_discarder_ = std::move(discarder);
}

// static
void PageDiscardingHelper::AddDiscardAttemptMarkerForTesting(
    PageNode* page_node) {
  DiscardAttemptMarker::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

// static
void PageDiscardingHelper::RemovesDiscardAttemptMarkerForTesting(
    PageNode* page_node) {
  DiscardAttemptMarker::Destroy(PageNodeImpl::FromNode(page_node));
}

void PageDiscardingHelper::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  graph->AddPageNodeObserver(this);
  graph->RegisterObject(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageDiscardingHelper::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->UnregisterObject(this);
  graph->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

const PageLiveStateDecorator::Data*
PageDiscardingHelper::GetPageNodeLiveStateData(
    const PageNode* page_node) const {
  return PageLiveStateDecorator::Data::FromPageNode(page_node);
}

PageDiscardingHelper::CanDiscardResult PageDiscardingHelper::CanDiscard(
    const PageNode* page_node,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) const {
  if (DiscardAttemptMarker::Get(PageNodeImpl::FromNode(page_node))) {
    return CanDiscardResult::kMarked;
  }

  bool is_proactive;
  switch (discard_reason) {
    case DiscardReason::EXTERNAL:
      // Always allow discards from external sources like extensions.
      return CanDiscardResult::kEligible;
    case DiscardReason::URGENT:
      is_proactive = false;
      break;
    case DiscardReason::PROACTIVE:
      is_proactive = true;
      break;
  }

  if (page_node->IsVisible()) {
    return CanDiscardResult::kProtected;
  }
  if (page_node->IsAudible()) {
    return CanDiscardResult::kProtected;
  }

  // Don't discard tabs that have recently played audio.
  auto it = last_change_to_non_audible_time_.find(page_node);
  if (it != last_change_to_non_audible_time_.end()) {
    if (base::TimeTicks::Now() - it->second < kTabAudioProtectionTime) {
      return CanDiscardResult::kProtected;
    }
  }

  if (page_node->GetTimeSinceLastVisibilityChange() <
      minimum_time_in_background) {
    return CanDiscardResult::kProtected;
  }

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  if (page_node->GetContentsMimeType() == "application/pdf") {
    return CanDiscardResult::kProtected;
  }

  // Don't discard tabs that don't have a main frame yet.
  auto* main_frame = page_node->GetMainFrameNode();
  if (!main_frame) {
    return CanDiscardResult::kProtected;
  }

  // Only discard http(s) pages and internal pages to make sure that we don't
  // discard extensions or other PageNode that don't correspond to a tab.
  bool is_web_page_or_internal_page =
      main_frame->GetURL().SchemeIsHTTPOrHTTPS() ||
      main_frame->GetURL().SchemeIs("chrome");
  if (!is_web_page_or_internal_page) {
    return CanDiscardResult::kProtected;
  }

  if (!main_frame->GetURL().is_valid() || main_frame->GetURL().is_empty()) {
    return CanDiscardResult::kProtected;
  }

  // The enterprise policy to except pages from discarding applies to both
  // proactive and urgent discards.
  if (IsPageOptedOutOfDiscarding(page_node->GetBrowserContextID(),
                                 main_frame->GetURL())) {
    return CanDiscardResult::kProtected;
  }

  const auto* live_state_data = GetPageNodeLiveStateData(page_node);

  // The live state data won't be available if none of these events ever
  // happened on the page.
  if (live_state_data) {
    // Don't discard the page if an extension is protecting it from discards.
    if (!live_state_data->IsAutoDiscardable()) {
      return CanDiscardResult::kProtected;
    }
    if (live_state_data->IsCapturingVideo()) {
      return CanDiscardResult::kProtected;
    }
    if (live_state_data->IsCapturingAudio()) {
      return CanDiscardResult::kProtected;
    }
    if (live_state_data->IsBeingMirrored()) {
      return CanDiscardResult::kProtected;
    }
    if (live_state_data->IsCapturingWindow()) {
      return CanDiscardResult::kProtected;
    }
    if (live_state_data->IsCapturingDisplay()) {
      return CanDiscardResult::kProtected;
    }
    if (live_state_data->IsConnectedToBluetoothDevice()) {
      return CanDiscardResult::kProtected;
    }
    if (live_state_data->IsConnectedToUSBDevice()) {
      return CanDiscardResult::kProtected;
    }
    // Don't discard the active tab in any window, even if the window is not
    // visible. Otherwise the user would see a blank page when the window
    // becomes visible again, as the tab isn't reloaded until they click on it.
    if (live_state_data->IsActiveTab()) {
      return CanDiscardResult::kProtected;
    }
    // Pinning a tab is a strong signal the user wants to keep it.
    if (live_state_data->IsPinnedTab()) {
      return CanDiscardResult::kProtected;
    }
    // Don't discard pages with devtools attached, because when it's restored
    // the devtools window won't come back. The user may be monitoring the page
    // in the background with devtools.
    if (live_state_data->IsDevToolsOpen()) {
      return CanDiscardResult::kProtected;
    }
    if (is_proactive) {
      if (live_state_data->IsContentSettingTypeAllowed(
              ContentSettingsType::NOTIFICATIONS)) {
        return CanDiscardResult::kProtected;
      }
      if (live_state_data->UpdatedTitleOrFaviconInBackground()) {
        return CanDiscardResult::kProtected;
      }
    }
#if !BUILDFLAG(IS_CHROMEOS)
    // TODO(sebmarchand): Skip this check if the Entreprise memory limit is set.
    if (live_state_data->WasDiscarded()) {
      return CanDiscardResult::kProtected;
    }
    // TODO(sebmarchand): Consider resetting the |WasDiscarded| value when the
    // main frame document changes, also remove the DiscardAttemptMarker in
    // this case.
#endif
  }

  // `HadUserEdits()` is currently a superset of `HadFormInteraction()` but
  // that may change so check both here (the check is not expensive).
  if (page_node->HadFormInteraction() || page_node->HadUserEdits()) {
    return CanDiscardResult::kProtected;
  }

  // TODO(sebmarchand): Do not discard crashed tabs.

  return CanDiscardResult::kEligible;
}

bool PageDiscardingHelper::IsPageOptedOutOfDiscarding(
    const std::string& browser_context_id,
    const GURL& url) const {
  if (!base::FeatureList::IsEnabled(features::kHighEfficiencyModeAvailable) &&
      !base::FeatureList::IsEnabled(features::kBatterySaverModeAvailable)) {
    // This list takes effect regardless of which mode the user is operating
    // under, but its launch is gated on these finch experiments for launch
    // considerations.
    return false;
  }

  auto it = profiles_no_discard_patterns_.find(browser_context_id);
  // TODO(crbug.com/1308741): Change the CHECK to a DCHECK in Sept 2022, after
  // verifying that there are no crash reports.
  CHECK(it != profiles_no_discard_patterns_.end());

  return !it->second->MatchURL(url).empty();
}

base::Value::Dict PageDiscardingHelper::DescribePageNodeData(
    const PageNode* node) const {
  auto can_discard = [this, node](DiscardReason discard_reason) {
    switch (this->CanDiscard(node, discard_reason, base::TimeDelta())) {
      case CanDiscardResult::kEligible:
        return "eligible";
      case CanDiscardResult::kProtected:
        return "protected";
      case CanDiscardResult::kMarked:
        return "marked";
    }
  };

  base::Value::Dict ret;
  ret.Set("can_urgently_discard", can_discard(DiscardReason::URGENT));
  ret.Set("can_proactively_discard", can_discard(DiscardReason::PROACTIVE));
  auto it = last_change_to_non_audible_time_.find(node);
  if (it != last_change_to_non_audible_time_.end()) {
    ret.Set("non_audible_change_time", TimeDeltaFromNowToValue(it->second));
  }
  auto* main_frame = node->GetMainFrameNode();
  if (main_frame) {
    ret.Set("opted_out", IsPageOptedOutOfDiscarding(node->GetBrowserContextID(),
                                                    main_frame->GetURL()));
  }
  return ret;
}

void PageDiscardingHelper::PostDiscardAttemptCallback(
    absl::optional<uint64_t> reclaim_target_kb,
    bool discard_protected_tabs,
    base::OnceCallback<void(bool)> post_discard_cb,
    DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background,
    bool success) {
  // When there is no discard candidate, DiscardMultiplePages returns
  // early and PostDiscardAttemptCallback is not called.
  if (!success) {
    // DiscardAttemptMarker will force the retry to choose different pages.
    DiscardMultiplePages(reclaim_target_kb, discard_protected_tabs,
                         std::move(post_discard_cb), discard_reason,
                         minimum_time_in_background);
    return;
  }

  std::move(post_discard_cb).Run(true);
}

}  // namespace policies
}  // namespace performance_manager

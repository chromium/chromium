// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
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
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {
namespace {

#if !BUILDFLAG(IS_CHROMEOS)
// Time during which non visible pages are protected from urgent discarding
// (not on ChromeOS).
constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::Minutes(10);
#endif

// Time during which a tab cannot be discarded after having played audio.
constexpr base::TimeDelta kTabAudioProtectionTime = base::Minutes(1);

// NodeAttachedData used to indicate that there's already been an attempt to
// discard a PageNode.
// TODO(sebmarchand): The only reason for a discard attempt to fail is if we try
// to discard a prerenderer, remove this once we can detect if a PageNode is a
// prerenderer in |CanUrgentlyDiscard|.
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

// Caches page node properties to facilitate sorting.
class PageNodeSortProxy {
 public:
  PageNodeSortProxy(const PageNode* page_node,
                    bool is_protected,
                    base::TimeDelta last_visible)
      : page_node_(page_node),
        is_protected_(is_protected),
        last_visible_(last_visible) {}
  const PageNode* page_node() { return page_node_; }

  // Returns true if the rhs is more important.
  bool operator<(const PageNodeSortProxy& rhs) const {
    if (is_protected_ && !rhs.is_protected_)
      return false;
    if (!is_protected_ && rhs.is_protected_)
      return true;
    return last_visible_ > rhs.last_visible_;
  }

 private:
  const PageNode* page_node_;
  bool is_protected_;
  // Delta between current time and last visibility change time.
  base::TimeDelta last_visible_;
};

using NodeRssMap = base::flat_map<const PageNode*, uint64_t>;

// Returns the mapping from page_node to its RSS estimation.
NodeRssMap GetPageNodeRssEstimateKb(
    const std::vector<PageNodeSortProxy>& candidates) {
  // Initialize the result map in one shot for time complexity O(n * log(n)).
  NodeRssMap::container_type result_container;
  result_container.reserve(candidates.size());
  for (auto candidate : candidates)
    result_container.emplace_back(candidate.page_node(), 0);
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
    if (!process_frames.size())
      continue;
    // Get the resident set of the process and split it equally across its
    // frames.
    const uint64_t frame_rss_kb =
        process_node->GetResidentSetKb() / process_frames.size();
    for (const FrameNode* frame_node : process_frames) {
      // Check if the frame belongs to a discardable page, if so update the
      // resident set of the page.
      auto iter = result.find(frame_node->GetPageNode());
      if (iter == result.end())
        continue;
      iter->second += frame_rss_kb;
    }
  }
  return result;
}

}  // namespace

PageDiscardingHelper::PageDiscardingHelper()
    : page_discarder_(std::make_unique<mechanism::PageDiscarder>()) {}
PageDiscardingHelper::~PageDiscardingHelper() = default;

void PageDiscardingHelper::UrgentlyDiscardAPage(
    features::DiscardStrategy discard_strategy,
    base::OnceCallback<void(bool)> post_discard_cb) {
  UrgentlyDiscardMultiplePages(absl::nullopt, discard_strategy, false,
                               std::move(post_discard_cb));
}

void PageDiscardingHelper::UrgentlyDiscardMultiplePages(
    absl::optional<uint64_t> reclaim_target_kb,
    features::DiscardStrategy discard_strategy,
    bool discard_protected_tabs,
    base::OnceCallback<void(bool)> post_discard_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(WARNING) << "Urgently discarding multiple pages with target (kb): "
               << (reclaim_target_kb ? *reclaim_target_kb : 0);

  // Ensures running post_discard_cb on early return.
  auto split_callback = base::SplitOnceCallback(std::move(post_discard_cb));
  base::ScopedClosureRunner run_post_discard_cb_on_return(
      base::BindOnce(std::move(split_callback.first), false));

  std::vector<const PageNode*> page_nodes = graph_->GetAllPageNodes();

  std::vector<PageNodeSortProxy> candidates;
  for (const auto* page_node : page_nodes) {
    CanUrgentlyDiscardResult can_discard_result = CanUrgentlyDiscard(page_node);
    if (can_discard_result == CanUrgentlyDiscardResult::kMarked)
      continue;
    bool is_protected =
        (can_discard_result == CanUrgentlyDiscardResult::kProtected);
    if (!discard_protected_tabs && is_protected)
      continue;
    candidates.emplace_back(page_node, is_protected,
                            page_node->GetTimeSinceLastVisibilityChange());
  }
  // Sorts with ascending importance.
  std::sort(candidates.begin(), candidates.end());

  UMA_HISTOGRAM_COUNTS_100("Discarding.DiscardCandidatesCount",
                           candidates.size());

  // Returns early when candidate is empty to avoid infinite loop in
  // UrgentlyDiscardMultiplePages and PostDiscardAttemptCallback.
  if (candidates.empty()) {
    return;
  }
  std::vector<const PageNode*> discard_attempts;

  if (discard_strategy == features::DiscardStrategy::LRU) {
    if (reclaim_target_kb == absl::nullopt) {
      const PageNode* oldest = candidates[0].page_node();
      discard_attempts.emplace_back(oldest);
    } else {
      const uint64_t reclaim_target_kb_value = *reclaim_target_kb;
      uint64_t total_reclaim_kb = 0;
      NodeRssMap page_node_rss_kb = GetPageNodeRssEstimateKb(candidates);
      for (auto& candidate : candidates) {
        if (total_reclaim_kb >= reclaim_target_kb_value)
          break;
        const PageNode* node = candidate.page_node();
        discard_attempts.emplace_back(node);
        // The node RSS value is updated by ProcessMetricsDecorator
        // periodically. The RSS value is 0 for nodes that have never been
        // updated, estimate the RSS value to 80 MiB for these nodes. 80 MiB is
        // the average Memory.Renderer.PrivateMemoryFootprint histogram value on
        // Windows in August 2021.
        total_reclaim_kb +=
            (page_node_rss_kb[node]) ? page_node_rss_kb[node] : 80 * 1024;
      }
    }
  } else if (discard_strategy == features::DiscardStrategy::BIGGEST_RSS) {
    if (reclaim_target_kb == absl::nullopt) {
      NodeRssMap page_node_rss_kb = GetPageNodeRssEstimateKb(candidates);
      DCHECK(!page_node_rss_kb.empty());

      const PageNode* oldest = candidates[0].page_node();
      auto largest =
          std::max_element(page_node_rss_kb.begin(), page_node_rss_kb.end(),
                           [](const std::pair<const PageNode*, uint64_t>& p1,
                              const std::pair<const PageNode*, uint64_t>& p2) {
                             return p1.second < p2.second;
                           });

      // max_element should return a valid element unless the map is empty.
      DCHECK(largest != page_node_rss_kb.end());

      if (largest->second != 0) {
        // Only report the memory usage metrics if we can compare them.
        UMA_HISTOGRAM_COUNTS_1000("Discarding.LargestTabFootprint",
                                  largest->second / 1024);
        UMA_HISTOGRAM_COUNTS_1000("Discarding.OldestTabFootprint",
                                  page_node_rss_kb[oldest] / 1024);
        discard_attempts.emplace_back(largest->first);
      } else {
        // If RSS is 0 for every node, fallback to the oldest node.
        discard_attempts.emplace_back(oldest);
      }
    } else {
      NOTIMPLEMENTED() << "When DiscardStrategy is BIGGEST_RSS, multiple "
                          "discarding is not supported.";
      return;
    }
  } else {
    NOTIMPLEMENTED() << "Unknown discard strategy.";
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
      discard_attempts,
      base::BindOnce(&PageDiscardingHelper::PostDiscardAttemptCallback,
                     weak_factory_.GetWeakPtr(), reclaim_target_kb,
                     discard_strategy, discard_protected_tabs,
                     std::move(split_callback.second)));
}

void PageDiscardingHelper::OnBeforePageNodeRemoved(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_change_to_non_audible_time_.erase(page_node);
}

void PageDiscardingHelper::OnIsAudibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!page_node->IsAudible())
    last_change_to_non_audible_time_[page_node] = base::TimeTicks::Now();
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

PageDiscardingHelper::CanUrgentlyDiscardResult
PageDiscardingHelper::CanUrgentlyDiscard(const PageNode* page_node) const {
  if (DiscardAttemptMarker::Get(PageNodeImpl::FromNode(page_node)))
    return CanUrgentlyDiscardResult::kMarked;

  if (page_node->IsVisible())
    return CanUrgentlyDiscardResult::kProtected;
  if (page_node->IsAudible())
    return CanUrgentlyDiscardResult::kProtected;

  // Don't discard tabs that have recently played audio.
  auto it = last_change_to_non_audible_time_.find(page_node);
  if (it != last_change_to_non_audible_time_.end()) {
    if (base::TimeTicks::Now() - it->second < kTabAudioProtectionTime)
      return CanUrgentlyDiscardResult::kProtected;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (page_node->GetTimeSinceLastVisibilityChange() <
      kNonVisiblePagesUrgentProtectionTime) {
    return CanUrgentlyDiscardResult::kProtected;
  }
#endif

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  if (page_node->GetContentsMimeType() == "application/pdf")
    return CanUrgentlyDiscardResult::kProtected;

  // Don't discard tabs that don't have a main frame yet.
  auto* main_frame = page_node->GetMainFrameNode();
  if (!main_frame)
    return CanUrgentlyDiscardResult::kProtected;

  // Only discard http(s) pages and internal pages to make sure that we don't
  // discard extensions or other PageNode that don't correspond to a tab.
  bool is_web_page_or_internal_page =
      main_frame->GetURL().SchemeIsHTTPOrHTTPS() ||
      main_frame->GetURL().SchemeIs("chrome");
  if (!is_web_page_or_internal_page)
    return CanUrgentlyDiscardResult::kProtected;

  if (!main_frame->GetURL().is_valid() || main_frame->GetURL().is_empty())
    return CanUrgentlyDiscardResult::kProtected;

  const auto* live_state_data = GetPageNodeLiveStateData(page_node);

  // The live state data won't be available if none of these events ever
  // happened on the page.
  if (live_state_data) {
    if (!live_state_data->IsAutoDiscardable())
      return CanUrgentlyDiscardResult::kProtected;
    if (live_state_data->IsCapturingVideo())
      return CanUrgentlyDiscardResult::kProtected;
    if (live_state_data->IsCapturingAudio())
      return CanUrgentlyDiscardResult::kProtected;
    if (live_state_data->IsBeingMirrored())
      return CanUrgentlyDiscardResult::kProtected;
    if (live_state_data->IsCapturingWindow())
      return CanUrgentlyDiscardResult::kProtected;
    if (live_state_data->IsCapturingDisplay())
      return CanUrgentlyDiscardResult::kProtected;
    if (live_state_data->IsConnectedToBluetoothDevice())
      return CanUrgentlyDiscardResult::kProtected;
    if (live_state_data->IsConnectedToUSBDevice())
      return CanUrgentlyDiscardResult::kProtected;
#if !BUILDFLAG(IS_CHROMEOS)
    // TODO(sebmarchand): Skip this check if the Entreprise memory limit is set.
    if (live_state_data->WasDiscarded())
      return CanUrgentlyDiscardResult::kProtected;
      // TODO(sebmarchand): Consider resetting the |WasDiscarded| value when the
      // main frame document changes, also remove the DiscardAttemptMarker in
      // this case.
#endif
  }

  if (page_node->HadFormInteraction())
    return CanUrgentlyDiscardResult::kProtected;

  // TODO(sebmarchand): Do not discard pages if they're connected to DevTools.

  // TODO(sebmarchand): Do not discard crashed tabs.

  // TODO(sebmarchand): Do not discard tabs that are the active ones in a tab
  // strip.

  // TODO(sebmarchand): Do not try to discard PageNode not attached to a tab
  // strip.

  return CanUrgentlyDiscardResult::kEligible;
}

base::Value PageDiscardingHelper::DescribePageNodeData(
    const PageNode* node) const {
  auto* data = DiscardAttemptMarker::Get(PageNodeImpl::FromNode(node));
  if (data == nullptr)
    return base::Value();

  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetKey("has_discard_attempt_marker", base::Value("true"));

  return ret;
}

void PageDiscardingHelper::PostDiscardAttemptCallback(
    absl::optional<uint64_t> reclaim_target_kb,
    features::DiscardStrategy discard_strategy,
    bool discard_protected_tabs,
    base::OnceCallback<void(bool)> post_discard_cb,
    bool success) {
  // When there is no discard candidate, UrgentlyDiscardMultiplePages returns
  // early and PostDiscardAttemptCallback is not called.
  if (!success) {
    // DiscardAttemptMarker will force the retry to choose different pages.
    UrgentlyDiscardMultiplePages(reclaim_target_kb, discard_strategy,
                                 discard_protected_tabs,
                                 std::move(post_discard_cb));
    return;
  }

  std::move(post_discard_cb).Run(true);
}

}  // namespace policies
}  // namespace performance_manager

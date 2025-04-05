// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/user_tuning/tab_revisit_tracker.h"

using performance_manager::mechanism::PageDiscarder;

namespace performance_manager::policies {
namespace {

BASE_FEATURE(kSkipDiscardsDrivenByStaleSignal,
             "SkipDiscardDrivenByStaleSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kDescriberName[] = "PageDiscardingHelper";

#if BUILDFLAG(IS_CHROMEOS)
// A 25% compression ratio is very conservative, and it matches the
// value used by resourced when calculating available memory.
static const uint64_t kSwapFootprintDiscount = 4;
#endif

using NodeFootprintMap = base::flat_map<const PageNode*, uint64_t>;

// Returns the mapping from page_node to its memory footprint estimation.
NodeFootprintMap GetPageNodeFootprintEstimateKb(
    const std::vector<PageNodeSortProxy>& candidates) {
  // Initialize the result map in one shot for time complexity O(n * log(n)).
  NodeFootprintMap::container_type result_container;
  result_container.reserve(candidates.size());
  for (auto candidate : candidates) {
    result_container.emplace_back(candidate.page_node(), 0);
  }
  NodeFootprintMap result(std::move(result_container));

  // TODO(crbug.com/40194476): Use visitor to accumulate the result to avoid
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
    ProcessNode::NodeSetView<const FrameNode*> process_frames =
        process_node->GetFrameNodes();
    if (!process_frames.size()) {
      continue;
    }
    // Get the footprint of the process and split it equally across its
    // frames.
    uint64_t footprint_kb = process_node->GetResidentSetKb();
#if BUILDFLAG(IS_CHROMEOS)
    footprint_kb += process_node->GetPrivateSwapKb() / kSwapFootprintDiscount;
#endif
    footprint_kb /= process_frames.size();
    for (const FrameNode* frame_node : process_frames) {
      // Check if the frame belongs to a discardable page, if so update the
      // resident set of the page.
      auto iter = result.find(frame_node->GetPageNode());
      if (iter == result.end()) {
        continue;
      }
      iter->second += footprint_kb;
    }
  }
  return result;
}

void RecordDiscardedTabMetrics(const PageNodeSortProxy& candidate) {
  // Logs a histogram entry to track the proportion of discarded tabs that
  // were protected at the time of discard.
  UMA_HISTOGRAM_BOOLEAN("Discarding.DiscardingProtectedTab",
                        candidate.is_protected());

  // Logs a histogram entry to track the proportion of discarded tabs that
  // were focused at the time of discard.
  UMA_HISTOGRAM_BOOLEAN("Discarding.DiscardingFocusedTab",
                        candidate.is_focused());
}

}  // namespace

PageDiscardingHelper::PageDiscardingHelper()
    : page_discarder_(std::make_unique<PageDiscarder>()) {}
PageDiscardingHelper::~PageDiscardingHelper() = default;

std::optional<base::TimeTicks> PageDiscardingHelper::DiscardAPage(
    DiscardEligibilityPolicy::DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  return DiscardMultiplePages(std::nullopt, false, discard_reason,
                              minimum_time_in_background);
}

std::optional<base::TimeTicks> PageDiscardingHelper::DiscardMultiplePages(
    std::optional<memory_pressure::ReclaimTarget> reclaim_target,
    bool discard_protected_tabs,
    DiscardEligibilityPolicy::DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reclaim_target) {
    if (base::FeatureList::IsEnabled(kSkipDiscardsDrivenByStaleSignal)) {
      reclaim_target =
          unnecessary_discard_monitor_.CorrectReclaimTarget(*reclaim_target);
    }

    unnecessary_discard_monitor_.OnReclaimTargetBegin(*reclaim_target);
  }

  LOG(WARNING) << "Discarding multiple pages with target (kb): "
               << (reclaim_target ? reclaim_target->target_kb : 0)
               << ", discard_protected_tabs: " << discard_protected_tabs;

  DiscardEligibilityPolicy* eligiblity_policy =
      DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());
  DCHECK(eligiblity_policy);

  std::vector<PageNodeSortProxy> candidates;
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    CanDiscardResult can_discard_result = eligiblity_policy->CanDiscard(
        page_node, discard_reason, minimum_time_in_background);
    if (can_discard_result == CanDiscardResult::kDisallowed) {
      continue;
    }
    if (can_discard_result == CanDiscardResult::kProtected &&
        !discard_protected_tabs) {
      continue;
    }
    candidates.emplace_back(page_node, can_discard_result,
                            page_node->IsVisible(), page_node->IsFocused(),
                            page_node->GetTimeSinceLastVisibilityChange());
  }

  // Sorts with ascending importance.
  std::sort(candidates.begin(), candidates.end());

  UMA_HISTOGRAM_COUNTS_100("Discarding.DiscardCandidatesCount",
                           candidates.size());

  // Returns early when candidate is empty to avoid infinite loop in
  // DiscardMultiplePages.
  if (candidates.empty()) {
    return std::nullopt;
  }
  std::vector<const PageNode*> discard_attempts;

  if (!reclaim_target) {
    const PageNode* oldest = candidates[0].page_node();
    discard_attempts.emplace_back(oldest);

    // Record metrics about the tab that is about to be discarded.
    RecordDiscardedTabMetrics(candidates[0]);
  } else {
    const uint64_t reclaim_target_kb_value = reclaim_target->target_kb;
    uint64_t total_reclaim_kb = 0;
    auto page_node_footprint_kb = GetPageNodeFootprintEstimateKb(candidates);
    for (auto& candidate : candidates) {
      if (total_reclaim_kb >= reclaim_target_kb_value) {
        break;
      }
      const PageNode* node = candidate.page_node();
      discard_attempts.emplace_back(node);

      // Record metrics about the tab that is about to be discarded.
      RecordDiscardedTabMetrics(candidate);

      // The node footprint value is updated by ProcessMetricsDecorator
      // periodically. The footprint value is 0 for nodes that have never been
      // updated, estimate the RSS value to 80 MiB for these nodes. 80 MiB is
      // the average Memory.Renderer.PrivateMemoryFootprint histogram value on
      // Windows in August 2021.
      uint64_t node_reclaim_kb = (page_node_footprint_kb[node])
                                     ? page_node_footprint_kb[node]
                                     : 80 * 1024;
      total_reclaim_kb += node_reclaim_kb;

      LOG(WARNING) << "Queueing discard attempt, type="
                   << performance_manager::PageNode::ToString(node->GetType())
                   << ", flags=[" << (candidate.is_focused() ? " focused" : "")
                   << (candidate.is_protected() ? " protected" : "")
                   << (candidate.is_visible() ? " visible" : "")
                   << " ] to save " << node_reclaim_kb << " KiB";
    }
  }

  // Clear the candidates vector to avoid holding on to pointers of the pages
  // that are about to be discarded.
  candidates.clear();

  if (discard_attempts.empty()) {
    // No pages left that are available for discarding.
    return std::nullopt;
  }

  // Adorns the PageNodes with a discard attempt marker to make sure that we
  // don't try to discard it multiple times if it fails to be discarded. In
  // practice this should only happen to prerenderers.
  for (auto* attempt : discard_attempts) {
    DiscardEligibilityPolicy::AddDiscardAttemptMarker(
        PageNodeImpl::FromNode(attempt));
  }

  std::vector<PageDiscarder::DiscardEvent> discard_events =
      page_discarder_->DiscardPageNodes(discard_attempts, discard_reason);

  if (discard_events.empty()) {
    // DiscardAttemptMarker will force the retry to choose different pages.
    return DiscardMultiplePages(reclaim_target, discard_protected_tabs,
                                discard_reason, minimum_time_in_background);
  }

  for (const auto& discard_event : discard_events) {
    unnecessary_discard_monitor_.OnDiscard(
        discard_event.estimated_memory_freed_kb, discard_event.discard_time);
  }

  unnecessary_discard_monitor_.OnReclaimTargetEnd();

  return discard_events[0].discard_time;
}

std::optional<base::TimeTicks>
PageDiscardingHelper::ImmediatelyDiscardMultiplePages(
    const std::vector<const PageNode*>& page_nodes,
    DiscardEligibilityPolicy::DiscardReason discard_reason) {
  // Pass 0 TimeDelta to bypass the minimum time in background check.
  return ImmediatelyDiscardMultiplePages(
      page_nodes, discard_reason,
      /*minimum_time_in_background=*/base::TimeDelta());
}

std::optional<base::TimeTicks>
PageDiscardingHelper::ImmediatelyDiscardMultiplePages(
    const std::vector<const PageNode*>& page_nodes,
    DiscardEligibilityPolicy::DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  DiscardEligibilityPolicy* eligiblity_policy =
      DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());
  DCHECK(eligiblity_policy);
  std::vector<const PageNode*> eligible_nodes;
  for (const PageNode* node : page_nodes) {
    if (eligiblity_policy->CanDiscard(node, discard_reason,
                                      minimum_time_in_background) ==
        CanDiscardResult::kEligible) {
      eligible_nodes.emplace_back(node);
    }
  }

  if (eligible_nodes.empty()) {
    return std::nullopt;
  } else {
    auto discard_events = page_discarder_->DiscardPageNodes(
        std::move(eligible_nodes), discard_reason);
    if (discard_events.size() > 0) {
      return discard_events[0].discard_time;
    }
    return std::nullopt;
  }
}

void PageDiscardingHelper::SetMockDiscarderForTesting(
    std::unique_ptr<PageDiscarder> discarder) {
  page_discarder_ = std::move(discarder);
}

void PageDiscardingHelper::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddPageNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageDiscardingHelper::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemovePageNodeObserver(this);
}

base::Value::Dict PageDiscardingHelper::DescribePageNodeData(
    const PageNode* node) const {
  base::Value::Dict ret;
  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(node);
  if (tab_handle) {
    TabRevisitTracker* revisit_tracker =
        GetOwningGraph()->GetRegisteredObjectAs<TabRevisitTracker>();
    CHECK(revisit_tracker);
    TabRevisitTracker::StateBundle state =
        revisit_tracker->GetStateForTabHandle(tab_handle);
    ret.Set("num_revisits", static_cast<int>(state.num_revisits));
  }

  return ret;
}

}  // namespace performance_manager::policies

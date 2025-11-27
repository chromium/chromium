// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/byte_count.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
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

using NodeFootprintMap = base::flat_map<const PageNode*, base::ByteCount>;

// Returns the mapping from page_node to its memory footprint estimation.
NodeFootprintMap GetPageNodeFootprintEstimate(
    const std::vector<PageNodeSortProxy>& candidates) {
  // Initialize the result map in one shot for time complexity O(n * log(n)).
  NodeFootprintMap::container_type result_container;
  result_container.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    result_container.emplace_back(candidate.page_node().get(),
                                  base::ByteCount(0));
  }
  NodeFootprintMap result(std::move(result_container));

  // TODO(crbug.com/40194476): Use visitor to accumulate the result to avoid
  // allocating extra lists of frame nodes behind the scenes.

  // List all the processes associated with these page nodes.
  base::flat_set<const ProcessNode*> process_nodes;
  for (const auto& candidate : candidates) {
    base::flat_set<const ProcessNode*> processes =
        GraphOperations::GetAssociatedProcessNodes(candidate.page_node().get());
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
    base::ByteCount footprint = process_node->GetResidentSet();
#if BUILDFLAG(IS_CHROMEOS)
    footprint += process_node->GetPrivateSwap() / kSwapFootprintDiscount;
#endif
    footprint /= process_frames.size();
    for (const FrameNode* frame_node : process_frames) {
      // Check if the frame belongs to a discardable page, if so update the
      // resident set of the page.
      auto iter = result.find(frame_node->GetPageNode());
      if (iter == result.end()) {
        continue;
      }
      iter->second += footprint;
    }
  }
  return result;
}

void RecordDiscardedTabMetrics(const PageNodeSortProxy& candidate) {
  // Logs a histogram entry to track the proportion of discarded tabs that
  // were protected at the time of discard.
  UMA_HISTOGRAM_BOOLEAN("Discarding.DiscardingProtectedTab2",
                        candidate.is_protected());

  // Logs a histogram entry to track the proportion of discarded tabs that
  // were focused at the time of discard.
  UMA_HISTOGRAM_BOOLEAN("Discarding.DiscardingFocusedTab2",
                        candidate.is_focused());
}

}  // namespace

PageDiscardingHelper::PageDiscardingHelper()
    : page_discarder_(std::make_unique<PageDiscarder>()) {}
PageDiscardingHelper::~PageDiscardingHelper() = default;

PageDiscardingHelper::DiscardResult PageDiscardingHelper::DiscardAPage(
    DiscardEligibilityPolicy::DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  return DiscardMultiplePagesImpl(std::nullopt, false, discard_reason,
                                  minimum_time_in_background);
}

std::optional<base::TimeTicks> PageDiscardingHelper::DiscardMultiplePages(
    std::optional<memory_pressure::ReclaimTarget> reclaim_target,
    bool discard_protected_tabs,
    DiscardEligibilityPolicy::DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  auto result =
      DiscardMultiplePagesImpl(reclaim_target, discard_protected_tabs,
                               discard_reason, minimum_time_in_background);
  return result.first_discard_time;
}

PageDiscardingHelper::DiscardResult
PageDiscardingHelper::DiscardMultiplePagesImpl(
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
               << (reclaim_target ? reclaim_target->target.InKiB() : 0)
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
    candidates.emplace_back(page_node->GetWeakPtr(), can_discard_result,
                            page_node->IsVisible(), page_node->IsFocused(),
                            page_node->GetLastVisibilityChangeTime());
  }

  // Sorts with descending importance.
  std::sort(candidates.rbegin(), candidates.rend());

  UMA_HISTOGRAM_COUNTS_100("Discarding.DiscardCandidatesCount",
                           candidates.size());

  // Estimate the memory footprint of each candidate to determine when enough
  // candidates have been discarded to reach the `reclaim_target`. This is not
  // needed when there is no `reclaim_target`.
  NodeFootprintMap page_node_footprint;
  if (reclaim_target) {
    // Only compute the estimated memory footprint if needed.
    page_node_footprint = GetPageNodeFootprintEstimate(candidates);
  }

  base::ByteCount total_reclaim;
  DiscardResult result;

  // Note: If `reclaim_target->target` is zero, this loop is not entered.
  while (!candidates.empty() &&
         (!reclaim_target || total_reclaim < reclaim_target->target)) {
    const PageNodeSortProxy candidate = std::move(candidates.back());
    candidates.pop_back();

    if (!candidate.page_node()) {
      // Skip if discarding another page caused this page to be deleted.
      continue;
    }

    const PageNode* node = candidate.page_node().get();

    std::optional<base::ByteCount> node_reclaim;
    if (reclaim_target) {
      // TODO(crbug.com/40755583): Use the `estimated_memory_freed` obtained
      // from `DiscardPageNode()` below to avoid the need to build
      // `page_node_footprint`.

      // The node footprint value is updated by ProcessMetricsDecorator
      // periodically. The footprint value is 0 for nodes that have never been
      // updated, estimate the RSS value to 80 MiB for these nodes. 80 MiB is
      // the average Memory.Renderer.PrivateMemoryFootprint histogram value on
      // Windows in August 2021.
      node_reclaim = page_node_footprint[node].is_zero()
                         ? base::MiB(80)
                         : page_node_footprint[node];

      LOG(WARNING) << "Queueing discard attempt, type="
                   << performance_manager::PageNode::ToString(node->GetType())
                   << ", flags=[" << (candidate.is_focused() ? " focused" : "")
                   << (candidate.is_protected() ? " protected" : "")
                   << (candidate.is_visible() ? " visible" : "")
                   << " ] to save " << node_reclaim.value();
    }

    // Adorn the PageNode with a discard attempt marker to make sure that we
    // don't try to discard it multiple times if it fails to be discarded. In
    // practice this should only happen to prerenderers.
    DiscardEligibilityPolicy::AddDiscardAttemptMarker(
        PageNodeImpl::FromNode(node));

    // PageNode may be replaced after discard. TabHandle is not replaced after
    // discard.
    TabPageDecorator::TabHandle* tab_handle;
    if (!result.first_discard_time.has_value()) {
      tab_handle = TabPageDecorator::FromPageNode(node);
    }

    // Do the discard.
    std::optional<base::ByteCount> estimated_memory_freed =
        page_discarder_->DiscardPageNode(node, discard_reason);

    // If discard is successful:
    if (estimated_memory_freed.has_value()) {
      const base::TimeTicks discard_time = base::TimeTicks::Now();

      unnecessary_discard_monitor_.OnDiscard(estimated_memory_freed.value(),
                                             discard_time);

      RecordDiscardedTabMetrics(candidate);

      // Update the time of the first successful discard.
      if (!result.first_discard_time.has_value()) {
        result.first_discard_time = discard_time;
        // Get the WebContents of the first discarded tab after discard.
        if (tab_handle) {
          const PageNode* node_after_discard = tab_handle->page_node();
          if (node_after_discard) {
            result.first_content_after_discard =
                node_after_discard->GetWebContents().get();
          }
        }
      }

      // Without a reclaim target: Return after the first successful discard.
      if (!reclaim_target) {
        return result;
      }

      // With a reclaim target: Update the amount of memory reclaimed, and loop
      // again.
      total_reclaim += node_reclaim.value();
    }
  }

  unnecessary_discard_monitor_.OnReclaimTargetEnd();

  return result;
}

bool PageDiscardingHelper::ImmediatelyDiscardMultiplePages(
    const std::vector<const PageNode*>& page_nodes,
    DiscardEligibilityPolicy::DiscardReason discard_reason,
    base::TimeDelta minimum_time_in_background) {
  DiscardEligibilityPolicy* eligibility_policy =
      DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());
  DCHECK(eligibility_policy);
  std::vector<base::WeakPtr<const PageNode>> eligible_nodes;
  for (const PageNode* node : page_nodes) {
    if (eligibility_policy->CanDiscard(node, discard_reason,
                                       minimum_time_in_background) ==
        CanDiscardResult::kEligible) {
      eligible_nodes.emplace_back(node->GetWeakPtr());
    }
  }

  bool had_successful_discard = false;

  for (base::WeakPtr<const PageNode> node : eligible_nodes) {
    // Skip if discarding another page caused this page to be deleted.
    if (!node) {
      continue;
    }

    had_successful_discard |=
        page_discarder_->DiscardPageNode(node.get(), discard_reason)
            .has_value();
  }

  return had_successful_discard;
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

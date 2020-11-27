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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Time during which non visible pages are protected from urgent discarding
// (not on ChromeOS).
constexpr base::TimeDelta kNonVisiblePagesUrgentProtectionTime =
    base::TimeDelta::FromMinutes(10);
#endif

// Time during which a tab cannot be discarded after having played audio.
constexpr base::TimeDelta kTabAudioProtectionTime =
    base::TimeDelta::FromMinutes(1);

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

}  // namespace

PageDiscardingHelper::PageDiscardingHelper()
    : page_discarder_(std::make_unique<mechanism::PageDiscarder>()) {}
PageDiscardingHelper::~PageDiscardingHelper() = default;

void PageDiscardingHelper::UrgentlyDiscardAPage(
    features::DiscardStrategy discard_strategy,
    base::OnceCallback<void(bool)> post_discard_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_map<const PageNode*, uint64_t> discardable_pages;
  base::TimeDelta oldest_bg_time;
  const PageNode* oldest_bg_discardable_page_node = nullptr;
  const PageNode* discard_candidate = nullptr;
  // Find all the pages that could be discarded.
  for (const auto* page_node : graph_->GetAllPageNodes()) {
    if (!CanUrgentlyDiscard(page_node))
      continue;
    discardable_pages.emplace(page_node, 0);
    // Track the discardable page that has been in background for the longest
    // period of time.
    if (page_node->GetTimeSinceLastVisibilityChange() > oldest_bg_time) {
      oldest_bg_time = page_node->GetTimeSinceLastVisibilityChange();
      oldest_bg_discardable_page_node = page_node;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Discarding.DiscardCandidatesCount",
                           discardable_pages.size());

  if (discardable_pages.empty()) {
    std::move(post_discard_cb).Run(false);
    return;
  }

  if (discard_strategy == features::DiscardStrategy::LRU) {
    discard_candidate = oldest_bg_discardable_page_node;
  } else if (discard_strategy == features::DiscardStrategy::BIGGEST_RSS) {
    // List all the processes associated with these page nodes.
    base::flat_set<const ProcessNode*> process_nodes;
    for (const auto& iter : discardable_pages) {
      auto processes = GraphOperations::GetAssociatedProcessNodes(iter.first);
      process_nodes.insert(processes.begin(), processes.end());
    }

    uint64_t largest_resident_set_kb = 0;
    const PageNode* largest_page_node = nullptr;
    // Compute the resident set of each page by simply summing up the estimated
    // resident set of all its frames, find the largest one.
    for (const ProcessNode* process_node : process_nodes) {
      auto process_frames = process_node->GetFrameNodes();
      uint64_t frame_rss_kb = 0;
      // Get the resident set of the process and split it equally across its
      // frames.
      if (process_frames.size())
        frame_rss_kb = process_node->GetResidentSetKb() / process_frames.size();
      for (const FrameNode* frame_node : process_frames) {
        // Check if the frame belongs to a discardable page, if so update the
        // resident set of the page.
        auto iter = discardable_pages.find(frame_node->GetPageNode());
        if (iter == discardable_pages.end())
          continue;
        iter->second += frame_rss_kb;
        if (iter->second > largest_resident_set_kb) {
          largest_resident_set_kb = iter->second;
          largest_page_node = iter->first;
        }
      }
    }
    if (largest_page_node) {
      // Only report the memory usage metrics if we can compare them.
      UMA_HISTOGRAM_COUNTS_1000("Discarding.LargestTabFootprint",
                                discardable_pages[largest_page_node] / 1024);
      UMA_HISTOGRAM_COUNTS_1000(
          "Discarding.OldestTabFootprint",
          discardable_pages[oldest_bg_discardable_page_node] / 1024);
      discard_candidate = largest_page_node;
    } else {
      discard_candidate = oldest_bg_discardable_page_node;
    }
  }

  // Adorns the PageNode with a discard attempt marker to make sure that we
  // don't try to discard it multiple times if it fails to be discarded. In
  // practice this should only happen to prerenderers.
  DiscardAttemptMarker::GetOrCreate(PageNodeImpl::FromNode(discard_candidate));

  page_discarder_->DiscardPageNode(
      discard_candidate,
      base::BindOnce(&PageDiscardingHelper::PostDiscardAttemptCallback,
                     weak_factory_.GetWeakPtr(), discard_strategy,
                     std::move(post_discard_cb)));
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

bool PageDiscardingHelper::CanUrgentlyDiscard(const PageNode* page_node) const {
  if (page_node->IsVisible())
    return false;
  if (page_node->IsAudible())
    return false;

  if (DiscardAttemptMarker::Get(PageNodeImpl::FromNode(page_node)))
    return false;

  // Don't discard tabs that have recently played audio.
  auto it = last_change_to_non_audible_time_.find(page_node);
  if (it != last_change_to_non_audible_time_.end()) {
    if (base::TimeTicks::Now() - it->second < kTabAudioProtectionTime)
      return false;
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (page_node->GetTimeSinceLastVisibilityChange() <
      kNonVisiblePagesUrgentProtectionTime) {
    return false;
  }
#endif

  // Do not discard PDFs as they might contain entry that is not saved and they
  // don't remember their scrolling positions. See crbug.com/547286 and
  // crbug.com/65244.
  if (page_node->GetContentsMimeType() == "application/pdf")
    return false;

  // Don't discard tabs that don't have a main frame yet.
  auto* main_frame = page_node->GetMainFrameNode();
  if (!main_frame)
    return false;

  // Only discard http(s) pages and internal pages to make sure that we don't
  // discard extensions or other PageNode that don't correspond to a tab.
  bool is_web_page_or_internal_page =
      main_frame->GetURL().SchemeIsHTTPOrHTTPS() ||
      main_frame->GetURL().SchemeIs("chrome");
  if (!is_web_page_or_internal_page)
    return false;

  if (!main_frame->GetURL().is_valid() || main_frame->GetURL().is_empty())
    return false;

  const auto* live_state_data = GetPageNodeLiveStateData(page_node);

  // The live state data won't be available if none of these events ever
  // happened on the page.
  if (live_state_data) {
    if (!live_state_data->IsAutoDiscardable())
      return false;
    if (live_state_data->IsCapturingVideo())
      return false;
    if (live_state_data->IsCapturingAudio())
      return false;
    if (live_state_data->IsBeingMirrored())
      return false;
    if (live_state_data->IsCapturingWindow())
      return false;
    if (live_state_data->IsCapturingDisplay())
      return false;
    if (live_state_data->IsConnectedToBluetoothDevice())
      return false;
    if (live_state_data->IsConnectedToUSBDevice())
      return false;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(sebmarchand): Skip this check if the Entreprise memory limit is set.
    if (live_state_data->WasDiscarded())
      return false;
      // TODO(sebmarchand): Consider resetting the |WasDiscarded| value when the
      // main frame document changes, also remove the DiscardAttemptMarker in
      // this case.
#endif
  }

  if (page_node->HadFormInteraction())
    return false;

  // TODO(sebmarchand): Do not discard pages if they're connected to DevTools.

  // TODO(sebmarchand): Do not discard crashed tabs.

  // TODO(sebmarchand): Do not discard tabs that are the active ones in a tab
  // strip.

  // TODO(sebmarchand): Do not try to discard PageNode not attached to a tab
  // strip.

  return true;
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
    features::DiscardStrategy discard_strategy,
    base::OnceCallback<void(bool)> post_discard_cb,
    bool success) {
  if (!success) {
    // Try to discard another page.
    UrgentlyDiscardAPage(discard_strategy, std::move(post_discard_cb));
    return;
  }

  std::move(post_discard_cb).Run(true);
}

}  // namespace policies
}  // namespace performance_manager

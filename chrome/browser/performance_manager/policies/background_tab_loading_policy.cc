// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"

#include "base/containers/contains.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "chrome/browser/performance_manager/policies/background_tab_loading_policy_helpers.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/policies/background_tab_loading_policy.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/common/url_constants.h"

namespace performance_manager {

namespace policies {

namespace {

// Pointer to the instance of itself.
BackgroundTabLoadingPolicy* g_background_tab_loading_policy = nullptr;

const char kDescriberName[] = "BackgroundTabLoadingPolicy";

}  // namespace

// static
constexpr uint32_t BackgroundTabLoadingPolicy::kMinTabsToLoad;
constexpr uint32_t BackgroundTabLoadingPolicy::kMaxTabsToLoad;
constexpr uint32_t BackgroundTabLoadingPolicy::kDesiredAmountOfFreeMemoryMb;
constexpr base::TimeDelta
    BackgroundTabLoadingPolicy::kMaxTimeSinceLastUseToLoad;
constexpr uint32_t BackgroundTabLoadingPolicy::kMinSimultaneousTabLoads;
constexpr uint32_t BackgroundTabLoadingPolicy::kMaxSimultaneousTabLoads;
constexpr uint32_t BackgroundTabLoadingPolicy::kCoresPerSimultaneousTabLoad;

void ScheduleLoadForRestoredTabs(
    std::vector<content::WebContents*> web_contents_vector) {
  std::vector<base::WeakPtr<PageNode>> weakptr_page_nodes;
  weakptr_page_nodes.reserve(web_contents_vector.size());
  for (auto* content : web_contents_vector) {
    weakptr_page_nodes.push_back(
        PerformanceManager::GetPageNodeForWebContents(content));
  }
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](std::vector<base::WeakPtr<PageNode>> weakptr_page_nodes,
                        performance_manager::Graph* graph) {
                       std::vector<PageNode*> page_nodes;
                       page_nodes.reserve(weakptr_page_nodes.size());
                       for (auto page_node : weakptr_page_nodes) {
                         // If the PageNode has been deleted before
                         // BackgroundTabLoading starts restoring it, then there
                         // is no need to restore it.
                         if (PageNode* raw_page = page_node.get())
                           page_nodes.push_back(raw_page);
                       }
                       BackgroundTabLoadingPolicy::GetInstance()
                           ->ScheduleLoadForRestoredTabs(std::move(page_nodes));
                     },
                     std::move(weakptr_page_nodes)));
}

BackgroundTabLoadingPolicy::BackgroundTabLoadingPolicy()
    : page_loader_(std::make_unique<mechanism::PageLoader>()) {
  DCHECK(!g_background_tab_loading_policy);
  g_background_tab_loading_policy = this;
  max_simultaneous_tab_loads_ = CalculateMaxSimultaneousTabLoads(
      kMinSimultaneousTabLoads, kMaxSimultaneousTabLoads,
      kCoresPerSimultaneousTabLoad, base::SysInfo::NumberOfProcessors());
}

BackgroundTabLoadingPolicy::~BackgroundTabLoadingPolicy() {
  DCHECK_EQ(this, g_background_tab_loading_policy);
  g_background_tab_loading_policy = nullptr;
}

void BackgroundTabLoadingPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
  graph->AddSystemNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void BackgroundTabLoadingPolicy::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveSystemNodeObserver(this);
  graph->RemovePageNodeObserver(this);
}

void BackgroundTabLoadingPolicy::OnLoadingStateChanged(
    const PageNode* page_node) {
  switch (page_node->GetLoadingState()) {
    // Loading is complete or stalled.
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadedIdle:
    case PageNode::LoadingState::kLoadingTimedOut:

    {
      // Stop tracking the page within this policy.
      RemovePageNode(page_node);

      // Since there might be a free loading slot, attempt to load more tabs.
      MaybeLoadSomeTabs();

      return;
    }

    // Loading starts.
    case PageNode::LoadingState::kLoading: {
      // The PageNode started loading because of this policy or because of
      // external factors (e.g. user-initiated). In either case, remove the
      // PageNode from the set of PageNodes for which a load needs to be
      // initiated and from the set of PageNodes for which a load has been
      // initiated but hasn't started.
      ErasePageNodeToLoadData(page_node);
      base::Erase(page_nodes_load_initiated_, page_node);

      // Keep track of all PageNodes that are loading, even when the load isn't
      // initiated by this policy.
      DCHECK(!base::Contains(page_nodes_loading_, page_node));
      page_nodes_loading_.push_back(page_node);

      return;
    }

    // Loading is progressing.
    case PageNode::LoadingState::kLoadedBusy: {
      // This PageNode should have been added to |page_nodes_loading_| when it
      // transitioned to |kLoading|.
      DCHECK(base::Contains(page_nodes_loading_, page_node));
      return;
    }
  }
}

void BackgroundTabLoadingPolicy::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  RemovePageNode(page_node);

  // There may be free loading slots, check and load more tabs if that's the
  // case.
  MaybeLoadSomeTabs();
}

void BackgroundTabLoadingPolicy::ScheduleLoadForRestoredTabs(
    std::vector<PageNode*> page_nodes) {
  for (auto* page_node : page_nodes) {
    // Put the |page_node| in the queue for loading.
    DCHECK(!FindPageNodeToLoadData(page_node));
    DCHECK(
        TabPropertiesDecorator::Data::FromPageNode(page_node)->IsInTabStrip());

    page_nodes_to_load_.push_back(
        std::make_unique<PageNodeToLoadData>(page_node));
  }

  for (auto& page_node_to_load_data : page_nodes_to_load_) {
    SetUsedInBackgroundAsync(page_node_to_load_data.get());
  }
}

void BackgroundTabLoadingPolicy::SetMockLoaderForTesting(
    std::unique_ptr<mechanism::PageLoader> loader) {
  page_loader_ = std::move(loader);
}

void BackgroundTabLoadingPolicy::SetMaxSimultaneousLoadsForTesting(
    size_t loading_slots) {
  max_simultaneous_tab_loads_ = loading_slots;
}

void BackgroundTabLoadingPolicy::SetFreeMemoryForTesting(
    size_t free_memory_mb) {
  free_memory_mb_for_testing_ = free_memory_mb;
}

void BackgroundTabLoadingPolicy::ResetPolicyForTesting() {
  tab_loads_started_ = 0;
}

BackgroundTabLoadingPolicy* BackgroundTabLoadingPolicy::GetInstance() {
  return g_background_tab_loading_policy;
}

BackgroundTabLoadingPolicy::PageNodeToLoadData::PageNodeToLoadData(
    PageNode* page_node)
    : page_node(page_node) {}
BackgroundTabLoadingPolicy::PageNodeToLoadData::~PageNodeToLoadData() = default;

struct BackgroundTabLoadingPolicy::ScoredTabComparator {
  bool operator()(const std::unique_ptr<PageNodeToLoadData>& tab0,
                  const std::unique_ptr<PageNodeToLoadData>& tab1) {
    // Greater scores sort first.
    return tab0->score > tab1->score;
  }
};

base::Value BackgroundTabLoadingPolicy::DescribePageNodeData(
    const PageNode* node) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (base::Contains(page_nodes_load_initiated_, node)) {
    // Transient state between InitiateLoad() and OnLoadingStateChanged(),
    // shouldn't be sticking around for long.
    dict.SetBoolKey("page_load_initiated", true);
  }
  if (base::Contains(page_nodes_loading_, node)) {
    dict.SetBoolKey("page_loading", true);
  }
  return !dict.DictEmpty() ? std::move(dict) : base::Value();
}

base::Value BackgroundTabLoadingPolicy::DescribeSystemNodeData(
    const SystemNode* node) const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("max_simultaneous_tab_loads",
                 base::saturated_cast<int>(max_simultaneous_tab_loads_));
  dict.SetIntKey("tab_loads_started",
                 base::saturated_cast<int>(tab_loads_started_));
  dict.SetIntKey("tabs_scored", base::saturated_cast<int>(tabs_scored_));
  return dict;
}

bool BackgroundTabLoadingPolicy::ShouldLoad(const PageNode* page_node) {
  if (tab_loads_started_ < kMinTabsToLoad)
    return true;

  if (tab_loads_started_ >= kMaxTabsToLoad)
    return false;

  // If there is a free memory constraint then enforce it.
  size_t free_memory_mb = GetFreePhysicalMemoryMib();
  if (free_memory_mb < kDesiredAmountOfFreeMemoryMb)
    return false;

  // Enforce a max time since last use.
  if (page_node->GetTimeSinceLastVisibilityChange() >
      kMaxTimeSinceLastUseToLoad) {
    return false;
  }

  // TODO(crbug.com/1071100): Enforce the site engagement score for tabs that
  // don't make use of background communication mechanisms.
  return true;
}

void BackgroundTabLoadingPolicy::OnUsedInBackgroundAvailable(
    base::WeakPtr<PageNode> page_node) {
  if (!page_node) {
    // Ignore the value if the PageNode was deleted.
    return;
  }
  PageNodeToLoadData* page_node_to_load_data =
      FindPageNodeToLoadData(page_node.get());
  if (!page_node_to_load_data) {
    // Ignore the value if the PageNode is no longer in the list of PageNodes to
    // load (it may already have started loading).
    return;
  }

  // TODO(crbug.com/1071100): Use real |used_in_bg| data from the database.
  DCHECK(!page_node_to_load_data->used_in_bg.has_value());
  page_node_to_load_data->used_in_bg = false;
  ++tabs_scored_;
  ScoreTab(page_node_to_load_data);
  DispatchNotifyAllTabsScoredIfNeeded();
}

void BackgroundTabLoadingPolicy::StopLoadingTabs() {
  // Clear out the remaining tabs to load and clean ourselves up.
  page_nodes_to_load_.clear();
  tabs_scored_ = 0;

  // TODO(crbug.com/1071077): Interrupt all ongoing loads.
}

void BackgroundTabLoadingPolicy::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  switch (new_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      StopLoadingTabs();
      break;
  }
}

void BackgroundTabLoadingPolicy::ScoreTab(
    PageNodeToLoadData* page_node_to_load_data) {
  DCHECK_EQ(page_node_to_load_data->score, 0.0f);
  float score = 0.0f;

  // Give higher priorities to tabs used in the background, and lowest
  // priority to internal tabs. Apps and pinned tabs are simply treated as
  // normal tabs.
  if (page_node_to_load_data->used_in_bg == true) {
    score = 2;
  } else if (!page_node_to_load_data->page_node->GetMainFrameUrl().SchemeIs(
                 content::kChromeUIScheme)) {
    score = 1;
  }

  // Refine the score using the age of the tab. More recently used tabs have
  // higher scores.
  score += CalculateAgeScore(
      page_node_to_load_data->page_node->GetTimeSinceLastVisibilityChange()
          .InSecondsF());

  page_node_to_load_data->score = score;
}

void BackgroundTabLoadingPolicy::SetUsedInBackgroundAsync(
    PageNodeToLoadData* page_node_to_load_data) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BackgroundTabLoadingPolicy::OnUsedInBackgroundAvailable,
          weak_factory_.GetWeakPtr(),
          std::move(PageNodeImpl::FromNode(page_node_to_load_data->page_node))
              ->GetWeakPtr()));
}

void BackgroundTabLoadingPolicy::DispatchNotifyAllTabsScoredIfNeeded() {
  if (tabs_scored_ == page_nodes_to_load_.size()) {
    NotifyAllTabsScored();
  }
}

void BackgroundTabLoadingPolicy::NotifyAllTabsScored() {
  std::stable_sort(page_nodes_to_load_.begin(), page_nodes_to_load_.end(),
                   ScoredTabComparator());
  MaybeLoadSomeTabs();
}

void BackgroundTabLoadingPolicy::InitiateLoad(const PageNode* page_node) {
  // Mark |page_node| as load initiated. Ensure that InitiateLoad is only called
  // for a PageNode that is tracked by the policy.
  ErasePageNodeToLoadData(page_node);
  DCHECK(!FindPageNodeToLoadData(page_node));
  page_nodes_load_initiated_.push_back(page_node);
  tab_loads_started_++;

  // Make the call to load |page_node|.
  page_loader_->LoadPageNode(page_node);
}

void BackgroundTabLoadingPolicy::RemovePageNode(const PageNode* page_node) {
  ErasePageNodeToLoadData(page_node);
  base::Erase(page_nodes_load_initiated_, page_node);
  base::Erase(page_nodes_loading_, page_node);
}

void BackgroundTabLoadingPolicy::MaybeLoadSomeTabs() {
  // Continue to load tabs while possible. This is in a loop with a
  // recalculation of GetMaxNewTabLoads() as reentrancy can cause conditions
  // to change as each tab load is initiated.
  while (GetMaxNewTabLoads() > 0)
    LoadNextTab();
}

size_t BackgroundTabLoadingPolicy::GetMaxNewTabLoads() const {
  // This takes into account all tabs currently loading across the browser,
  // including ones that BackgroundTabLoadingPolicy isn't explicitly managing.
  // This ensures that BackgroundTabLoadingPolicy respects user interaction
  // first and foremost. There's a small race between when we initiated loading
  // and when PageNodeObserver notifies us that it has actually started, so we
  // also make use of |page_nodes_initiated_| to track these.
  size_t loading_tab_count =
      page_nodes_load_initiated_.size() + page_nodes_loading_.size();

  // Determine the number of free loading slots available.
  size_t page_nodes_to_load = 0;
  if (loading_tab_count < max_simultaneous_tab_loads_)
    page_nodes_to_load = max_simultaneous_tab_loads_ - loading_tab_count;

  // Cap the number of loads by the actual number of tabs remaining.
  page_nodes_to_load = std::min(page_nodes_to_load, page_nodes_to_load_.size());

  return page_nodes_to_load;
}

void BackgroundTabLoadingPolicy::LoadNextTab() {
  DCHECK(!page_nodes_to_load_.empty());

  // Find the next PageNode to load.
  while (!page_nodes_to_load_.empty()) {
    const PageNode* page_node = page_nodes_to_load_.front()->page_node;
    if (ShouldLoad(page_node)) {
      InitiateLoad(page_node);
      return;
    }

    // |page_node| should not be loaded at this time. Remove |page_node| from
    // the policy.
    ErasePageNodeToLoadData(page_node);
  }
}

size_t BackgroundTabLoadingPolicy::GetFreePhysicalMemoryMib() const {
  if (free_memory_mb_for_testing_ != 0)
    return free_memory_mb_for_testing_;
  constexpr int64_t kMibibytesInBytes = 1 << 20;
  int64_t free_mem =
      base::SysInfo::AmountOfAvailablePhysicalMemory() / kMibibytesInBytes;
  DCHECK_GE(free_mem, 0);
  return free_mem;
}

void BackgroundTabLoadingPolicy::ErasePageNodeToLoadData(
    const PageNode* page_node) {
  for (auto& page_node_to_load_data : page_nodes_to_load_) {
    if (page_node_to_load_data->page_node == page_node) {
      if (page_node_to_load_data->used_in_bg.has_value()) {
        // If the PageNode has already been scored, remove it from the
        // |tabs_scored_| count.
        --tabs_scored_;
        base::Erase(page_nodes_to_load_, page_node_to_load_data);
      } else {
        base::Erase(page_nodes_to_load_, page_node_to_load_data);

        // If the PageNode has not been scored yet, then removing it may trigger
        // all tabs scored notification.
        DispatchNotifyAllTabsScoredIfNeeded();
      }
      return;
    }
  }
}

BackgroundTabLoadingPolicy::PageNodeToLoadData*
BackgroundTabLoadingPolicy::FindPageNodeToLoadData(const PageNode* page_node) {
  for (auto& page_node_to_load_data : page_nodes_to_load_) {
    if (page_node_to_load_data->page_node == page_node) {
      return page_node_to_load_data.get();
    }
  }
  return nullptr;
}

}  // namespace policies

}  // namespace performance_manager

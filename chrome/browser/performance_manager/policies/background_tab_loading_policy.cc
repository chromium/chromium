// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "chrome/browser/performance_manager/policies/background_tab_loading_policy_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/policies/background_tab_loading_policy.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

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

BackgroundTabLoadingPolicy::PageNodeData::PageNodeData(
    base::WeakPtr<PageNode> page_node,
    GURL main_frame_url,
    blink::mojom::PermissionStatus notification_permission_status)
    : page_node(std::move(page_node)),
      main_frame_url(std::move(main_frame_url)),
      notification_permission_status(notification_permission_status) {}

BackgroundTabLoadingPolicy::PageNodeData::PageNodeData(PageNodeData&& other) =
    default;
BackgroundTabLoadingPolicy::PageNodeData&
BackgroundTabLoadingPolicy::PageNodeData::operator=(PageNodeData&& other) =
    default;
BackgroundTabLoadingPolicy::PageNodeData::PageNodeData(
    const PageNodeData& other) = default;
BackgroundTabLoadingPolicy::PageNodeData&
BackgroundTabLoadingPolicy::PageNodeData::operator=(const PageNodeData& other) =
    default;
BackgroundTabLoadingPolicy::PageNodeData::~PageNodeData() = default;

void ScheduleLoadForRestoredTabs(
    std::vector<content::WebContents*> web_contents_vector) {
  DCHECK(!web_contents_vector.empty());

  std::vector<BackgroundTabLoadingPolicy::PageNodeData> page_node_data_vector;
  page_node_data_vector.reserve(web_contents_vector.size());
  for (content::WebContents* content : web_contents_vector) {
    content::PermissionController* permission_controller =
        content->GetBrowserContext()->GetPermissionController();

    // Cannot use GetPermissionStatusForCurrentDocument() because the navigation
    // hasn't been committed in the RenderFrameHost yet, as evidenced by the
    // DCHECK below.
    DCHECK_EQ(content->GetPrimaryMainFrame()->GetLastCommittedURL(), GURL());
    DCHECK_NE(content->GetLastCommittedURL(), GURL());

    page_node_data_vector.emplace_back(
        PerformanceManager::GetPrimaryPageNodeForWebContents(content),
        content->GetLastCommittedURL(),
        permission_controller
            ->GetPermissionResultForOriginWithoutContext(
                blink::PermissionType::NOTIFICATIONS,
                url::Origin::Create(content->GetLastCommittedURL()))
            .status);
  }

  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<BackgroundTabLoadingPolicy::PageNodeData>
                 page_node_data_vector,
             performance_manager::Graph* graph) {
            BackgroundTabLoadingPolicy::GetInstance()
                ->ScheduleLoadForRestoredTabs(std::move(page_node_data_vector));
          },
          std::move(page_node_data_vector)));
}

BackgroundTabLoadingPolicy::BackgroundTabLoadingPolicy(
    base::RepeatingClosure all_restored_tabs_loaded_callback)
    : all_restored_tabs_loaded_callback_(
          std::move(all_restored_tabs_loaded_callback)),
      page_loader_(std::make_unique<mechanism::PageLoader>()) {
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
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  DCHECK_EQ(has_restored_tabs_to_load_, HasRestoredTabsToLoad());

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
      if (previous_state == PageNode::LoadingState::kLoadedBusy) {
        // The PageNode remained in |page_nodes_loading_| when it transitioned
        // from |kLoading| to |kLoadedBusy|, so no change is necessary when it
        // transitions back to |kLoading|.
        DCHECK(base::Contains(page_nodes_loading_, page_node));
        DCHECK(!base::Contains(page_nodes_load_initiated_, page_node));
        DCHECK(!FindPageNodeToLoadData(page_node));
        return;
      }

      // The PageNode started loading because of this policy or because of
      // external factors (e.g. user-initiated). In either case, remove the
      // PageNode from the set of PageNodes for which a load needs to be
      // initiated and from the set of PageNodes for which a load has been
      // initiated but hasn't started.
      const bool erased =
          ErasePageNodeToLoadData(page_node) ||
          std::erase(page_nodes_load_initiated_, page_node) != 0;

      // Keep track of all PageNodes that are loading, even when the load isn't
      // initiated by this policy.
      DCHECK(!base::Contains(page_nodes_loading_, page_node));
      page_nodes_loading_.emplace(page_node, erased);

      return;
    }

    // Loading is progressing.
    case PageNode::LoadingState::kLoadedBusy: {
      // The PageNode should have been added to |page_nodes_loading_| when it
      // transitioned to |kLoading|.
      DCHECK(base::Contains(page_nodes_loading_, page_node));
      DCHECK(!base::Contains(page_nodes_load_initiated_, page_node));
      DCHECK(!FindPageNodeToLoadData(page_node));
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
    std::vector<BackgroundTabLoadingPolicy::PageNodeData>
        page_node_data_vector) {
  has_restored_tabs_to_load_ = true;

  const size_t page_nodes_to_load_initial_size = page_nodes_to_load_.size();

  for (const auto& page_node_data : page_node_data_vector) {
    PageNode* page_node = page_node_data.page_node.get();
    if (!page_node)
      continue;

    DCHECK_EQ(page_node->GetType(), PageType::kTab);
    DCHECK(!FindPageNodeToLoadData(page_node));
    DCHECK(!base::Contains(page_nodes_load_initiated_, page_node));

    // Setting main frame restored state ensures that the notification
    // permission status and background title/favicon update properties are set
    // correctly when `ScoreTab` scores the page.
    PageNodeImpl::FromNode(page_node)->SetMainFrameRestoredState(
        page_node_data.main_frame_url,
        page_node_data.notification_permission_status);

    // No need to schedule a load if the page is already loading.
    if (base::Contains(page_nodes_loading_, page_node)) {
      // Track that this policy was responsible for scheduling the load.
      page_nodes_loading_[page_node] = true;
      continue;
    }

    // Put the page in the queue for loading.
    page_nodes_to_load_.push_back(
        std::make_unique<PageNodeToLoadData>(page_node));
  }

  // Asynchronously determine whether pages added to `page_nodes_to_load_` are
  // used in background. Do this after all pages have been added to
  // `page_nodes_to_load_`, otherwise the policy may start loading pages without
  // knowing about all the tabs that must be loaded (see
  // `OnUsedInBackgroundAvailable()`).
  for (size_t i = page_nodes_to_load_initial_size;
       i < page_nodes_to_load_.size(); ++i) {
    SetUsedInBackgroundAsync(page_nodes_to_load_[i].get());
  }

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();
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
    DCHECK(tab0->score.has_value());
    DCHECK(tab1->score.has_value());
    // Greater scores sort first.
    return tab0->score > tab1->score;
  }
};

base::Value::Dict BackgroundTabLoadingPolicy::DescribePageNodeData(
    const PageNode* node) const {
  base::Value::Dict dict;
  if (base::Contains(page_nodes_load_initiated_, node)) {
    // Transient state between InitiateLoad() and OnLoadingStateChanged(),
    // shouldn't be sticking around for long.
    dict.Set("page_load_initiated", true);
  }
  if (base::Contains(page_nodes_loading_, node)) {
    dict.Set("page_loading", true);
  }
  return !dict.empty() ? std::move(dict) : base::Value::Dict();
}

base::Value::Dict BackgroundTabLoadingPolicy::DescribeSystemNodeData(
    const SystemNode* node) const {
  base::Value::Dict dict;
  dict.Set("max_simultaneous_tab_loads",
           base::saturated_cast<int>(max_simultaneous_tab_loads_));
  dict.Set("tab_loads_started", base::saturated_cast<int>(tab_loads_started_));
  dict.Set("tabs_scored", base::saturated_cast<int>(tabs_scored_));
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

  // TODO(crbug.com/40126611): Enforce the site engagement score for tabs that
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

  SiteDataReader* reader = GetSiteDataReader(page_node.get());

  // A tab can't play audio until it has been visible at least once so
  // UsesAudioInBackground() is ignored.
  page_node_to_load_data->updates_title_or_favicon_in_bg =
      reader && (reader->UpdatesFaviconInBackground() !=
                     SiteFeatureUsage::kSiteFeatureNotInUse ||
                 reader->UpdatesTitleInBackground() !=
                     SiteFeatureUsage::kSiteFeatureNotInUse);

  ScoreTab(page_node_to_load_data);
  DispatchNotifyAllTabsScoredIfNeeded();
}

void BackgroundTabLoadingPolicy::StopLoadingTabs() {
  // Clear out the remaining tabs to load and clean ourselves up.
  page_nodes_to_load_.clear();
  tabs_scored_ = 0;

  // TODO(crbug.com/40126598): Interrupt all ongoing loads.

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();
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

SiteDataReader* BackgroundTabLoadingPolicy::GetSiteDataReader(
    const PageNode* page_node) const {
  return SiteDataRecorder::Data::GetReaderForPageNode(page_node);
}

void BackgroundTabLoadingPolicy::ScoreTab(
    PageNodeToLoadData* page_node_to_load_data) {
  DCHECK(!page_node_to_load_data->score.has_value());
  float score = 0.0f;

  // Give higher priorities to tabs used in the background, and lowest
  // priority to internal tabs. Apps and pinned tabs are simply treated as
  // normal tabs.
  if (page_node_to_load_data->page_node->GetNotificationPermissionStatus() ==
          blink::mojom::PermissionStatus::GRANTED ||
      page_node_to_load_data->updates_title_or_favicon_in_bg.value()) {
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

  ++tabs_scored_;
  page_node_to_load_data->score = score;
}

void BackgroundTabLoadingPolicy::SetUsedInBackgroundAsync(
    PageNodeToLoadData* page_node_to_load_data) {
  const PageNode* page_node = page_node_to_load_data->page_node.get();
  SiteDataReader* reader = GetSiteDataReader(page_node);
  auto callback =
      base::BindOnce(&BackgroundTabLoadingPolicy::OnUsedInBackgroundAvailable,
                     weak_factory_.GetWeakPtr(),
                     PageNodeImpl::FromNode(page_node)->GetWeakPtr());

  // The tab won't have a reader if it doesn't have an URL tracked in the
  // site data database.
  if (!reader) {
    std::move(callback).Run();
    return;
  }

  reader->RegisterDataLoadedCallback(std::move(callback));
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
  // The page shouldn't already be loading.
  DCHECK(!base::Contains(page_nodes_load_initiated_, page_node));
  DCHECK(!base::Contains(page_nodes_loading_, page_node));

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
  std::erase(page_nodes_load_initiated_, page_node);
  page_nodes_loading_.erase(page_node);

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();
}

void BackgroundTabLoadingPolicy::MaybeLoadSomeTabs() {
  // Continue to load tabs while possible. This is in a loop with a
  // recalculation of GetMaxNewTabLoads() as reentrancy can cause conditions
  // to change as each tab load is initiated.
  while (GetMaxNewTabLoads() > 0)
    LoadNextTab();

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();
}

size_t BackgroundTabLoadingPolicy::GetMaxNewTabLoads() const {
  // Can't load tabs until all tabs have been scored.
  if (tabs_scored_ < page_nodes_to_load_.size())
    return 0U;

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
  DCHECK_EQ(tabs_scored_, page_nodes_to_load_.size());

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
  constexpr uint64_t kMibibytesInBytes = 1 << 20;
  return base::SysInfo::AmountOfAvailablePhysicalMemory() / kMibibytesInBytes;
}

bool BackgroundTabLoadingPolicy::ErasePageNodeToLoadData(
    const PageNode* page_node) {
  for (auto& page_node_to_load_data : page_nodes_to_load_) {
    if (page_node_to_load_data->page_node == page_node) {
      if (page_node_to_load_data->score.has_value()) {
        // If the PageNode has already been scored, remove it from the
        // |tabs_scored_| count.
        DCHECK_GT(tabs_scored_, 0U);
        --tabs_scored_;
        std::erase(page_nodes_to_load_, page_node_to_load_data);
      } else {
        std::erase(page_nodes_to_load_, page_node_to_load_data);

        // If the PageNode has not been scored yet, then removing it may trigger
        // all tabs scored notification.
        DispatchNotifyAllTabsScoredIfNeeded();
      }
      return true;
    }
  }
  return false;
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

bool BackgroundTabLoadingPolicy::HasRestoredTabsToLoad() const {
  if (!page_nodes_to_load_.empty())
    return true;
  if (!page_nodes_load_initiated_.empty())
    return true;
  for (const auto& [_, load_initiated_by_this] : page_nodes_loading_) {
    if (load_initiated_by_this)
      return true;
  }
  return false;
}

void BackgroundTabLoadingPolicy::UpdateHasRestoredTabsToLoad() {
  if (!has_restored_tabs_to_load_) {
    DCHECK(!HasRestoredTabsToLoad());
    return;
  }
  if (HasRestoredTabsToLoad())
    return;
  has_restored_tabs_to_load_ = false;
  all_restored_tabs_loaded_callback_.Run();
}

}  // namespace policies

}  // namespace performance_manager

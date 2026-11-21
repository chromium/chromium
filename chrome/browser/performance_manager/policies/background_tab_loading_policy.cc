// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory_coordinator/utils.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "chrome/browser/performance_manager/policies/background_tab_loading_policy_helpers.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/public/background_tab_loading_policy.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace performance_manager {

namespace policies {

namespace {

const char kDescriberName[] = "BackgroundTabLoadingPolicy";

constexpr base::MemoryConsumerTraits kBackgroundTabLoadingPolicyTraits(
    base::MemoryConsumerTraits::ConsumerType::kPassive,
    // Prevents allocations in renderer processes (out-of-process).
    base::MemoryConsumerTraits::InProcess::kNo);

#if BUILDFLAG(IS_ANDROID)
// On Android, BrowserMemoryCoordinator may be destroyed during test or process
// teardown before the PerformanceManager graph is destroyed.
constexpr auto kBackgroundTabLoadingPolicyCheckUnregister =
    base::MemoryConsumerRegistration::CheckUnregister::kDisabled;
#else
constexpr auto kBackgroundTabLoadingPolicyCheckUnregister =
    base::MemoryConsumerRegistration::CheckUnregister::kEnabled;
#endif

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
constexpr base::TimeDelta
    BackgroundTabLoadingPolicy::kMaxVisibleLoadingDeferralTimeout;

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

bool CanScheduleLoadForRestoredTabs() {
  return BackgroundTabLoadingPolicy::GetFromGraph();
}

void ScheduleLoadForRestoredTabs(
    std::vector<content::WebContents*> web_contents_vector) {
  // Trigger a slow-reports and collect a session restore trace if needed.
  base::trace_event::EmitNamedTrigger("session-restore-config");
  TRACE_EVENT("browser", "ScheduleLoadForRestoredTabs", "tabs_count",
              web_contents_vector.size());

  CHECK(!web_contents_vector.empty(), base::NotFatalUntil::M163);

  std::vector<BackgroundTabLoadingPolicy::PageNodeData> page_node_data_vector;
  page_node_data_vector.reserve(web_contents_vector.size());
  for (content::WebContents* content : web_contents_vector) {
    // Restore the favicon for deferred tabs to have some visual indication of
    // its contents.
    if (favicon::ContentFaviconDriver* favicon_driver =
            favicon::ContentFaviconDriver::FromWebContents(content);
        favicon_driver) {
      favicon_driver->FetchFavicon(favicon_driver->GetActiveURL(),
                                   /*is_same_document=*/false);
    }

    content::PermissionController* permission_controller =
        content->GetBrowserContext()->GetPermissionController();

    // Cannot use GetPermissionStatusForCurrentDocument() because the navigation
    // hasn't been committed in the RenderFrameHost yet, as evidenced by the
    // DCHECK below.
    // TODO(crbug.com/583678251): CHECK-exclusion: Convert to a CHECK once we
    // are confident it won't be triggered.
    DCHECK_EQ(content->GetPrimaryMainFrame()->GetLastCommittedURL(), GURL());
    CHECK_NE(content->GetLastCommittedURL(), GURL(), base::NotFatalUntil::M163);

    auto permission_descriptor = content::PermissionDescriptorUtil::
        CreatePermissionDescriptorForPermissionType(
            blink::PermissionType::NOTIFICATIONS);
    auto notification_permission =
        permission_controller
            ->GetPermissionResultForOriginWithoutContext(
                permission_descriptor,
                url::Origin::Create(content->GetLastCommittedURL()))
            .status;

    page_node_data_vector.emplace_back(
        PerformanceManager::GetPrimaryPageNodeForWebContents(content),
        content->GetLastCommittedURL(), notification_permission);
  }

  auto* policy = BackgroundTabLoadingPolicy::GetFromGraph();
  CHECK(policy);
  policy->ScheduleLoadForRestoredTabs(std::move(page_node_data_vector));
}

void InstallBackgroundTabLoadingPolicyForTesting(
    base::RepeatingClosure all_restored_tabs_loaded_callback) {
  CHECK(!BackgroundTabLoadingPolicy::GetFromGraph());
  PerformanceManager::GetGraph()->PassToGraph(
      std::make_unique<BackgroundTabLoadingPolicy>(
          std::move(all_restored_tabs_loaded_callback)));
}

void SetMaxLoadedBackgroundTabCountForTesting(size_t max_tabs_to_load) {
  auto* policy = BackgroundTabLoadingPolicy::GetFromGraph();
  CHECK(policy);
  policy->SetMaxLoadedTabCountForTesting(max_tabs_to_load);  // IN-TEST
}

void SetMaxSimultaneousBackgroundTabLoadsForTesting(size_t loading_slots) {
  auto* policy = BackgroundTabLoadingPolicy::GetFromGraph();
  CHECK(policy);
  policy->SetMaxSimultaneousLoadsForTesting(loading_slots);  // IN-TEST
}

BackgroundTabLoadingPolicy::BackgroundTabLoadingPolicy(
    base::RepeatingClosure all_restored_tabs_loaded_callback)
    : all_restored_tabs_loaded_callback_(
          std::move(all_restored_tabs_loaded_callback)),
      page_loader_(std::make_unique<mechanism::PageLoader>()),
      memory_consumer_registration_(
          "BackgroundTabLoadingPolicy",
          kBackgroundTabLoadingPolicyTraits,
          this,
          kBackgroundTabLoadingPolicyCheckUnregister) {
  max_simultaneous_tab_loads_ = CalculateMaxSimultaneousTabLoads(
      kMinSimultaneousTabLoads, kMaxSimultaneousTabLoads,
      kCoresPerSimultaneousTabLoad, base::SysInfo::NumberOfProcessors());
}

BackgroundTabLoadingPolicy::~BackgroundTabLoadingPolicy() = default;

void BackgroundTabLoadingPolicy::OnPassedToGraph(Graph* graph) {
  CHECK(!has_restored_tabs_to_load_);
  graph->AddPageNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void BackgroundTabLoadingPolicy::OnTakenFromGraph(Graph* graph) {
  if (has_restored_tabs_to_load_) {
    RecordTabOutcomes(TabLoadOutcome::kClosedBeforeLoading,
                      page_nodes_to_load_.size());
    RecordRestoreTabCountMetrics();
  }
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemovePageNodeObserver(this);
}

void BackgroundTabLoadingPolicy::OnIsVisibleChanged(const PageNode* page_node) {
  if (page_nodes_to_load_.empty() || !page_nodes_loading_.contains(page_node) ||
      !base::FeatureList::IsEnabled(
          features::kDeferBackgroundTabLoadsWhileVisiblePageLoads)) {
    return;
  }
  if (page_node->IsVisible()) {
    UpdateVisiblePageLoadDeferralTimeout();
    return;
  }
  // Post asynchronously because switching tabs detaches and hides the
  // outgoing active WebContents before attaching and starting load on the
  // incoming active WebContents within the same task.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundTabLoadingPolicy::MaybeLoadSomeTabs,
                                weak_factory_.GetWeakPtr()));
}

void BackgroundTabLoadingPolicy::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  TRACE_EVENT("browser", "BackgroundTabLoadingPolicy::OnLoadingStateChanged");
  CHECK_EQ(has_restored_tabs_to_load_, HasRestoredTabsToLoad(),
           base::NotFatalUntil::M163);

  switch (page_node->GetLoadingState()) {
    // Loading is complete or stalled.
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadedIdle:
    case PageNode::LoadingState::kLoadingTimedOut:

    {
      // Stop tracking the page within this policy.
      RemovePageNode(page_node, TabLoadOutcome::kLoadedExternally);

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
        CHECK(page_nodes_loading_.contains(page_node),
              base::NotFatalUntil::M163);
        CHECK(!std::ranges::contains(page_nodes_load_initiated_, page_node),
              base::NotFatalUntil::M163);
        CHECK(!FindPageNodeToLoadData(page_node), base::NotFatalUntil::M163);
        return;
      }

      // The PageNode started loading because of this policy or because of
      // external factors (e.g. user-initiated). In either case, remove the
      // PageNode from the set of PageNodes for which a load needs to be
      // initiated and from the set of PageNodes for which a load has been
      // initiated but hasn't started.
      const bool erased_page_node_to_load =
          ErasePageNodeToLoadData(page_node, TabLoadOutcome::kLoadedExternally);
      const bool erased =
          erased_page_node_to_load ||
          std::erase(page_nodes_load_initiated_, page_node) != 0;

      // Keep track of all PageNodes that are loading, even when the load isn't
      // initiated by this policy.
      CHECK(!page_nodes_loading_.contains(page_node),
            base::NotFatalUntil::M163);
      page_nodes_loading_.emplace(page_node, erased);

      if (erased_page_node_to_load) {
        // Removing a page to load may result in all those remaining being
        // scored.
        DispatchNotifyAllTabsScoredIfNeeded();
      }

      UpdateVisiblePageLoadDeferralTimeout();

      return;
    }

    // Loading is progressing.
    case PageNode::LoadingState::kLoadedBusy: {
      // The PageNode should have been added to |page_nodes_loading_| when it
      // transitioned to |kLoading|.
      CHECK(page_nodes_loading_.contains(page_node), base::NotFatalUntil::M163);
      CHECK(!std::ranges::contains(page_nodes_load_initiated_, page_node),
            base::NotFatalUntil::M163);
      CHECK(!FindPageNodeToLoadData(page_node), base::NotFatalUntil::M163);
      return;
    }
  }
}

void BackgroundTabLoadingPolicy::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  TRACE_EVENT("browser", "BackgroundTabLoadingPolicy::OnBeforePageNodeRemoved");
  RemovePageNode(page_node, TabLoadOutcome::kClosedBeforeLoading);

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

    CHECK_EQ(page_node->GetType(), PageType::kTab, base::NotFatalUntil::M163);
    CHECK(!FindPageNodeToLoadData(page_node), base::NotFatalUntil::M163);
    CHECK(!std::ranges::contains(page_nodes_load_initiated_, page_node),
          base::NotFatalUntil::M163);

    // Setting main frame restored state ensures that the notification
    // permission status and background title/favicon update properties are set
    // correctly when `ScoreTab` scores the page.
    PageNodeImpl::FromNode(page_node)->SetMainFrameRestoredState(
        page_node_data.main_frame_url,
        page_node_data.notification_permission_status);

    ++restore_tab_counts_.num_tabs_scheduled;

    // No need to schedule a load if the page is already loading.
    if (page_nodes_loading_.contains(page_node)) {
      RecordTabOutcomes(TabLoadOutcome::kAlreadyLoading, 1);
      // Make `HasRestoredTabsToLoad()` return true until this tab finishes
      // loading.
      page_nodes_loading_[page_node] = true;
      continue;
    }

    // Put the page in the queue for loading.
    page_nodes_to_load_.emplace_back(page_node);
  }

  // Asynchronously determine whether pages added to `page_nodes_to_load_` are
  // used in background. Do this after all pages have been added to
  // `page_nodes_to_load_`, otherwise the policy may start loading pages without
  // knowing about all the tabs that must be loaded (see
  // `OnUsedInBackgroundAvailable()`).
  for (size_t i = page_nodes_to_load_initial_size;
       i < page_nodes_to_load_.size(); ++i) {
    SetUsedInBackgroundAsync(page_nodes_to_load_[i].page_node);
  }

  UpdateVisiblePageLoadDeferralTimeout();

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();
}

void BackgroundTabLoadingPolicy::SetMockLoaderForTesting(
    std::unique_ptr<mechanism::PageLoader> loader) {
  page_loader_ = std::move(loader);
}

void BackgroundTabLoadingPolicy::SetMaxLoadedTabCountForTesting(
    size_t max_tabs_to_load) {
  max_tabs_to_load_ = max_tabs_to_load;
  if (min_tabs_to_load_ > max_tabs_to_load_) {
    min_tabs_to_load_ = max_tabs_to_load_;
  }
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

BackgroundTabLoadingPolicy::PageNodeToLoadData::PageNodeToLoadData(
    const PageNode* page_node)
    : page_node(page_node) {}

BackgroundTabLoadingPolicy::PageNodeToLoadData::~PageNodeToLoadData() = default;

bool BackgroundTabLoadingPolicy::PageNodeToLoadData::
    UsesBackgroundCommunication() const {
  return page_node->GetNotificationPermissionStatus() ==
             blink::mojom::PermissionStatus::GRANTED ||
         updates_title_or_favicon_in_bg.value();
}

struct BackgroundTabLoadingPolicy::ScoredTabComparator {
  bool operator()(const PageNodeToLoadData& tab0,
                  const PageNodeToLoadData& tab1) {
    CHECK(tab0.score.has_value(), base::NotFatalUntil::M163);
    CHECK(tab1.score.has_value(), base::NotFatalUntil::M163);
    // Greater scores sort first.
    return tab0.score > tab1.score;
  }
};

base::DictValue BackgroundTabLoadingPolicy::DescribePageNodeData(
    const PageNode* node) const {
  base::DictValue dict;
  if (std::ranges::contains(page_nodes_load_initiated_, node)) {
    // Transient state between InitiateLoad() and OnLoadingStateChanged(),
    // shouldn't be sticking around for long.
    dict.Set("page_load_initiated", true);
  }
  if (page_nodes_loading_.contains(node)) {
    dict.Set("page_loading", true);
  }
  return !dict.empty() ? std::move(dict) : base::DictValue();
}

base::DictValue BackgroundTabLoadingPolicy::DescribeSystemNodeData(
    const SystemNode* node) const {
  base::DictValue dict;
  dict.Set("max_simultaneous_tab_loads",
           base::saturated_cast<int>(max_simultaneous_tab_loads_));
  dict.Set("tab_loads_started", base::saturated_cast<int>(tab_loads_started_));
  dict.Set("tabs_scored", base::saturated_cast<int>(tabs_scored_));
  return dict;
}

BackgroundTabLoadingPolicy::LoadDecision BackgroundTabLoadingPolicy::ShouldLoad(
    const PageNodeToLoadData& page_node_data) const {
  if (tab_loads_started_ < min_tabs_to_load_) {
    return LoadDecision::kLoad;
  }

  if (tab_loads_started_ >= max_tabs_to_load_) {
    return LoadDecision::kSkipForMaxTabs;
  }

  // If there is a free memory constraint then enforce it.
  size_t free_memory_mb = GetFreePhysicalMemoryMib();
  if (free_memory_mb < kDesiredAmountOfFreeMemoryMb) {
    return LoadDecision::kSkipForFreeMemory;
  }

  // Enforce a max time since last use.
  const base::TimeDelta time_since_last_visibility_change =
      base::TimeTicks::Now() -
      page_node_data.page_node->GetVisibilityState().last_change_time;
  if (time_since_last_visibility_change > kMaxTimeSinceLastUseToLoad) {
    return LoadDecision::kSkipForTabAge;
  }

  return LoadDecision::kLoad;
}

void BackgroundTabLoadingPolicy::OnUsedInBackgroundAvailable(
    base::WeakPtr<PageNode> page_node) {
  TRACE_EVENT("browser",
              "BackgroundTabLoadingPolicy::OnUsedInBackgroundAvailable");
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
  TRACE_EVENT("browser", "BackgroundTabLoadingPolicy::StopLoadingTabs");
  // Clear out the remaining tabs to load and clean ourselves up.
  RecordTabOutcomes(TabLoadOutcome::kStoppedForMemoryPressure,
                    page_nodes_to_load_.size());
  page_nodes_to_load_.clear();
  tabs_scored_ = 0;
  UpdateVisiblePageLoadDeferralTimeout();

  // Ideally we'd interrupt all ongoing loads (see crbug.com/40126598)

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();
}

void BackgroundTabLoadingPolicy::OnUpdateMemoryLimit() {
  TRACE_EVENT_INSTANT(
      "browser", "BackgroundTabLoadingPolicy::OnUpdateMemoryLimitStopLoading",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* debug = event->add_debug_annotations();
        debug->set_name("memory_limit");
        debug->set_int_value(memory_limit().percent());
      });
  if (memory_limit() <= base::MemoryLimit::ModeratePressureThreshold()) {
    StopLoadingTabs();
  }
}

SiteDataReader* BackgroundTabLoadingPolicy::GetSiteDataReader(
    const PageNode* page_node) const {
  return SiteDataRecorder::Data::GetReaderForPageNode(page_node);
}

void BackgroundTabLoadingPolicy::ScoreTab(
    PageNodeToLoadData* page_node_to_load_data) {
  CHECK(!page_node_to_load_data->score.has_value(), base::NotFatalUntil::M163);
  float score = 0.0f;

  // Give higher priorities to tabs used in the background, and lowest
  // priority to internal tabs. Apps and pinned tabs are simply treated as
  // normal tabs.
  if (page_node_to_load_data->UsesBackgroundCommunication()) {
    score = 2;
  } else if (!page_node_to_load_data->page_node->GetMainFrameUrl().SchemeIs(
                 content::kChromeUIScheme)) {
    score = 1;
  }

  // Refine the score using the age of the tab. More recently used tabs have
  // higher scores.
  const base::TimeDelta time_since_last_visibility_change =
      base::TimeTicks::Now() -
      page_node_to_load_data->page_node->GetVisibilityState().last_change_time;
  score += CalculateAgeScore(time_since_last_visibility_change.InSecondsF());

  ++tabs_scored_;
  page_node_to_load_data->score = score;
}

void BackgroundTabLoadingPolicy::SetUsedInBackgroundAsync(
    const PageNode* page_node) {
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
  TRACE_EVENT("browser", "BackgroundTabLoadingPolicy::InitiateLoad");
  for (const PageNode* to_load : page_loader_->GetPageNodesToLoad(page_node)) {
    // Extra page nodes that weren't passed to ScheduleLoadForRestoredTabs() may
    // already be loading.
    if (to_load != page_node && page_nodes_loading_.contains(to_load)) {
      CHECK(!std::ranges::contains(page_nodes_load_initiated_, to_load),
            base::NotFatalUntil::M163);
      continue;
    }
    InitiateSinglePageLoad(to_load);
  }
}

void BackgroundTabLoadingPolicy::InitiateSinglePageLoad(
    const PageNode* page_node) {
  // The page shouldn't already be loading.
  CHECK(!std::ranges::contains(page_nodes_load_initiated_, page_node),
        base::NotFatalUntil::M163);
  CHECK(!page_nodes_loading_.contains(page_node), base::NotFatalUntil::M163);
  CHECK_EQ(tabs_scored_, page_nodes_to_load_.size(), base::NotFatalUntil::M163);

  // Move `page_node` from `page_nodes_to_load_` to
  // `page_nodes_load_initiated_`. `page_node` may be absent from
  // `page_nodes_to_load_` when `PageLoader::GetPageNodesToLoad()` returns a
  // split-view partner tab not passed to `ScheduleLoadForRestoredTabs()`.
  ErasePageNodeToLoadData(page_node, TabLoadOutcome::kLoadedByPolicy);
  CHECK(!FindPageNodeToLoadData(page_node), base::NotFatalUntil::M163);
  page_nodes_load_initiated_.push_back(page_node);
  tab_loads_started_++;

  // Make the call to load |page_node|.
  page_loader_->LoadPageNode(page_node);

  // No need to call DispatchNotifyAllTabsScoredIfNeeded() following
  // ErasePageNodeToLoadData() - all pages to load were already scored at the
  // beginning of this method.
}

void BackgroundTabLoadingPolicy::RemovePageNode(
    const PageNode* page_node,
    TabLoadOutcome outcome_if_queued) {
  const bool erased_page_node_to_load_data =
      ErasePageNodeToLoadData(page_node, outcome_if_queued);
  std::erase(page_nodes_load_initiated_, page_node);
  page_nodes_loading_.erase(page_node);
  UpdateVisiblePageLoadDeferralTimeout();

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();

  if (erased_page_node_to_load_data) {
    // Removing a page to load may result in all those remaining being scored.
    DispatchNotifyAllTabsScoredIfNeeded();
  }
}

void BackgroundTabLoadingPolicy::MaybeLoadSomeTabs() {
  TRACE_EVENT("browser", "BackgroundTabLoadingPolicy::MaybeLoadSomeTabs");

  // Continue to load tabs while possible. This is in a loop with a
  // recalculation of GetMaxNewTabLoads() as reentrancy can cause conditions
  // to change as each tab load is initiated.
  while (GetMaxNewTabLoads() > 0) {
    LoadNextTab();
  }

  UpdateVisiblePageLoadDeferralTimeout();

  // All restored tabs may be loaded.
  UpdateHasRestoredTabsToLoad();
}

size_t BackgroundTabLoadingPolicy::GetMaxNewTabLoads() const {
  // Can't load tabs until there are tabs to load and all tabs have been scored.
  if (page_nodes_to_load_.empty() ||
      tabs_scored_ < page_nodes_to_load_.size()) {
    return 0U;
  }

  // Defer starting background tab loads while any visible page is still in
  // `page_nodes_loading_` (`kLoading` or `kLoadedBusy` prior to CPU/network
  // quiescence), up to `kMaxVisibleLoadingDeferralTimeout`, so background tab
  // navigation and renderer creation do not contend with the active foreground
  // tab during startup/session restore.
  if (!has_visible_page_load_timed_out_ &&
      base::FeatureList::IsEnabled(
          features::kDeferBackgroundTabLoadsWhileVisiblePageLoads) &&
      HasLoadingVisiblePage()) {
    return 0U;
  }

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

bool BackgroundTabLoadingPolicy::HasLoadingVisiblePage() const {
  for (const auto& [page_node, _] : page_nodes_loading_) {
    if (page_node->IsVisible()) {
      return true;
    }
  }
  return false;
}

void BackgroundTabLoadingPolicy::UpdateVisiblePageLoadDeferralTimeout() {
  if (page_nodes_to_load_.empty() ||
      !base::FeatureList::IsEnabled(
          features::kDeferBackgroundTabLoadsWhileVisiblePageLoads) ||
      !HasLoadingVisiblePage()) {
    has_visible_page_load_timed_out_ = false;
    visible_page_load_timeout_timer_.Stop();
    return;
  }
  if (!has_visible_page_load_timed_out_ &&
      !visible_page_load_timeout_timer_.IsRunning()) {
    visible_page_load_timeout_timer_.Start(
        FROM_HERE, kMaxVisibleLoadingDeferralTimeout,
        base::BindOnce(&BackgroundTabLoadingPolicy::OnVisiblePageLoadTimedOut,
                       base::Unretained(this)));
  }
}

void BackgroundTabLoadingPolicy::OnVisiblePageLoadTimedOut() {
  has_visible_page_load_timed_out_ = true;
  MaybeLoadSomeTabs();
}

void BackgroundTabLoadingPolicy::LoadNextTab() {
  TRACE_EVENT("browser", "BackgroundTabLoadingPolicy::LoadNextTab");
  CHECK(!page_nodes_to_load_.empty(), base::NotFatalUntil::M163);
  CHECK_EQ(tabs_scored_, page_nodes_to_load_.size(), base::NotFatalUntil::M163);

  // Find the next PageNode to load.
  while (!page_nodes_to_load_.empty()) {
    const PageNode* page_node = page_nodes_to_load_.front().page_node;
    switch (ShouldLoad(page_nodes_to_load_.front())) {
      case LoadDecision::kLoad:
        InitiateLoad(page_node);
        return;
      case LoadDecision::kSkipForMaxTabs:
        ErasePageNodeToLoadData(page_node, TabLoadOutcome::kSkippedForMaxTabs);
        break;
      case LoadDecision::kSkipForFreeMemory:
        ErasePageNodeToLoadData(page_node,
                                TabLoadOutcome::kSkippedForFreeMemory);
        break;
      case LoadDecision::kSkipForTabAge:
        ErasePageNodeToLoadData(page_node, TabLoadOutcome::kSkippedForTabAge);
        break;
    }

    // No need to call DispatchNotifyAllTabsScoredIfNeeded() following
    // ErasePageNodeToLoadData() - all pages to load were already scored at the
    // beginning of this method.
  }
}

size_t BackgroundTabLoadingPolicy::GetFreePhysicalMemoryMib() const {
  if (free_memory_mb_for_testing_ != 0)
    return free_memory_mb_for_testing_;
  return base::SysInfo::AmountOfAvailablePhysicalMemory().InMiB();
}

bool BackgroundTabLoadingPolicy::ErasePageNodeToLoadData(
    const PageNode* page_node,
    TabLoadOutcome outcome) {
  for (auto it = page_nodes_to_load_.begin(); it != page_nodes_to_load_.end();
       ++it) {
    if (it->page_node == page_node) {
      if (it->score.has_value()) {
        // If the PageNode has already been scored, remove it from the
        // |tabs_scored_| count.
        CHECK_GT(tabs_scored_, 0U, base::NotFatalUntil::M163);
        --tabs_scored_;
      }

      page_nodes_to_load_.erase(it);
      RecordTabOutcomes(outcome, 1);
      return true;
    }
  }
  return false;
}

void BackgroundTabLoadingPolicy::RecordTabOutcomes(TabLoadOutcome outcome,
                                                   size_t count) {
  restore_tab_counts_.num_tabs_per_outcome[static_cast<size_t>(outcome)] +=
      count;
}

void BackgroundTabLoadingPolicy::RecordRestoreTabCountMetrics() {
  RestoreTabCounts counts = std::exchange(restore_tab_counts_, {});
  if (counts.num_tabs_scheduled == 0) {
    return;
  }

  // Every scheduled tab must have recorded an outcome (see invariant on
  // `RestoreTabCounts`).
  const size_t num_tabs_with_outcome =
      std::accumulate(counts.num_tabs_per_outcome.begin(),
                      counts.num_tabs_per_outcome.end(), size_t{0});
  CHECK_EQ(counts.num_tabs_scheduled, num_tabs_with_outcome);

  base::UmaHistogramCounts1000(
      "SessionRestore.BackgroundTabLoading.TabCount",
      base::saturated_cast<int>(counts.num_tabs_scheduled));

  // LINT.IfChange(TabLoadOutcomeHistogramNames)
  auto get_histogram_name = [](TabLoadOutcome outcome) -> std::string_view {
    switch (outcome) {
      case TabLoadOutcome::kLoadedByPolicy:
        return "SessionRestore.BackgroundTabLoading.TabCount.LoadedByPolicy";
      case TabLoadOutcome::kAlreadyLoading:
        return "SessionRestore.BackgroundTabLoading.TabCount.AlreadyLoading";
      case TabLoadOutcome::kLoadedExternally:
        return "SessionRestore.BackgroundTabLoading.TabCount.LoadedExternally";
      case TabLoadOutcome::kClosedBeforeLoading:
        return "SessionRestore.BackgroundTabLoading.TabCount."
               "ClosedBeforeLoading";
      case TabLoadOutcome::kSkippedForMaxTabs:
        return "SessionRestore.BackgroundTabLoading.TabCount.SkippedForMaxTabs";
      case TabLoadOutcome::kSkippedForFreeMemory:
        return "SessionRestore.BackgroundTabLoading.TabCount."
               "SkippedForFreeMemory";
      case TabLoadOutcome::kSkippedForTabAge:
        return "SessionRestore.BackgroundTabLoading.TabCount.SkippedForTabAge";
      case TabLoadOutcome::kStoppedForMemoryPressure:
        return "SessionRestore.BackgroundTabLoading.TabCount."
               "StoppedForMemoryPressure";
    }
  };
  // LINT.ThenChange(//chrome/browser/performance_manager/policies/background_tab_loading_policy.h:TabLoadOutcome)

  for (size_t i = 0; i < counts.num_tabs_per_outcome.size(); ++i) {
    base::UmaHistogramCounts1000(
        get_histogram_name(static_cast<TabLoadOutcome>(i)),
        base::saturated_cast<int>(counts.num_tabs_per_outcome[i]));
  }
}

BackgroundTabLoadingPolicy::PageNodeToLoadData*
BackgroundTabLoadingPolicy::FindPageNodeToLoadData(const PageNode* page_node) {
  for (auto& page_node_to_load_data : page_nodes_to_load_) {
    if (page_node_to_load_data.page_node == page_node) {
      return &page_node_to_load_data;
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
    CHECK(!HasRestoredTabsToLoad(), base::NotFatalUntil::M163);
    return;
  }
  if (HasRestoredTabsToLoad())
    return;
  has_restored_tabs_to_load_ = false;
  RecordRestoreTabCountMetrics();
  all_restored_tabs_loaded_callback_.Run();
}

}  // namespace policies

}  // namespace performance_manager

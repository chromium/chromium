// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/sessions/session_restore.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/swap_metrics_driver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

const char* const kSessionTypeName[] = {"SessionRestore",
                                        "BackgroundTabOpening"};

constexpr int kSamplingOdds = 10;

// Only report a subset of this metric as the volume is too high.
bool ShouldReportExpectedTaskQueueingDurationToUKM(
    size_t background_tab_loading_count,
    size_t background_tab_pending_count) {
  size_t tab_count =
      background_tab_loading_count + background_tab_pending_count;
  DCHECK_GE(tab_count, 1u);

  // We always collect this metric when we have 2 or more backgrounded loading
  // or pending tabs (|tab_count|). And we sample the rest, i.e. when there is
  // one tab loading in the background and no tabs pending, which is the less
  // interesting majority. In this way, we cap the volume while keeping all
  // interesting data.
  if (tab_count > 1)
    return true;

  if (base::RandUint64() % kSamplingOdds == 0)
    return true;

  return false;
}

ukm::SourceId GetUkmSourceId(content::WebContents* contents) {
  resource_coordinator::ResourceCoordinatorTabHelper* observer =
      resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          contents);
  if (!observer)
    return ukm::kInvalidSourceId;

  return observer->ukm_source_id();
}

}  // namespace

void TabManagerStatsCollector::BackgroundTabCountStats::Reset() {
  tab_count = 0u;
  tab_paused_count = 0u;
  tab_load_auto_started_count = 0u;
  tab_load_user_initiated_count = 0u;
}

class TabManagerStatsCollector::SwapMetricsDelegate
    : public content::SwapMetricsDriver::Delegate {
 public:
  explicit SwapMetricsDelegate(
      TabManagerStatsCollector* tab_manager_stats_collector,
      SessionType type)
      : tab_manager_stats_collector_(tab_manager_stats_collector),
        session_type_(type) {}

  ~SwapMetricsDelegate() override = default;

  void OnSwapInCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "SwapInPerSecond", count, interval);
  }

  void OnSwapOutCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "SwapOutPerSecond", count, interval);
  }

  void OnDecompressedPageCount(uint64_t count,
                               base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "DecompressedPagesPerSecond", count, interval);
  }

  void OnCompressedPageCount(uint64_t count,
                             base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        session_type_, "CompressedPagesPerSecond", count, interval);
  }

  void OnUpdateMetricsFailed() override {
    tab_manager_stats_collector_->OnUpdateSwapMetricsFailed();
  }

 private:
  TabManagerStatsCollector* tab_manager_stats_collector_;
  const SessionType session_type_;
};

TabManagerStatsCollector::TabManagerStatsCollector() {
  SessionRestore::AddObserver(this);

  // Post BEST_EFFORT task (which will only run after startup is completed) that
  // starts the periodic sampling of freezing and discarding stats.
  content::BrowserThread::PostBestEffortTask(
      FROM_HERE, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&TabManagerStatsCollector::StartPeriodicSampling,
                     weak_factory_.GetWeakPtr()));
}

TabManagerStatsCollector::~TabManagerStatsCollector() {
  SessionRestore::RemoveObserver(this);
}

void TabManagerStatsCollector::RecordSwitchToTab(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (!is_session_restore_loading_tabs_ &&
      !is_in_background_tab_opening_session_) {
    return;
  }

  if (IsInOverlappedSession())
    return;

  auto* new_data = TabManager::WebContentsData::FromWebContents(new_contents);
  DCHECK(new_data);

  if (is_session_restore_loading_tabs_) {
    UMA_HISTOGRAM_ENUMERATION(
        kHistogramSessionRestoreSwitchToTab,
        static_cast<int32_t>(new_data->tab_loading_state()),
        static_cast<int32_t>(LoadingState::kMaxValue) + 1);
  }
  if (is_in_background_tab_opening_session_) {
    UMA_HISTOGRAM_ENUMERATION(
        kHistogramBackgroundTabOpeningSwitchToTab,
        static_cast<int32_t>(new_data->tab_loading_state()),
        static_cast<int32_t>(LoadingState::kMaxValue) + 1);
  }

  if (old_contents)
    foreground_contents_switched_to_times_.erase(old_contents);
  DCHECK(!base::Contains(foreground_contents_switched_to_times_, new_contents));
  if (new_data->tab_loading_state() != LoadingState::LOADED) {
    foreground_contents_switched_to_times_.insert(
        std::make_pair(new_contents, NowTicks()));
  }
}

void TabManagerStatsCollector::RecordExpectedTaskQueueingDuration(
    content::WebContents* contents,
    base::TimeDelta queueing_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(fdoray): Consider not recording this for occluded tabs.
  if (contents->GetVisibility() == content::Visibility::HIDDEN)
    return;

  if (IsInOverlappedSession())
    return;

  ukm::SourceId ukm_source_id = GetUkmSourceId(contents);

  if (is_session_restore_loading_tabs_) {
    UMA_HISTOGRAM_TIMES(
        kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration,
        queueing_time);

    size_t restored_tab_count =
        g_browser_process->GetTabManager()->restored_tab_count();
    if (ukm_source_id != ukm::kInvalidSourceId && restored_tab_count > 1) {
      ukm::builders::
          TabManager_SessionRestore_ForegroundTab_ExpectedTaskQueueingDurationInfo(
              ukm_source_id)
              .SetExpectedTaskQueueingDuration(queueing_time.InMilliseconds())
              .SetSequenceId(sequence_++)
              .SetSessionRestoreSessionId(session_id_)
              .SetSessionRestoreTabCount(restored_tab_count)
              .SetSystemTabCount(
                  g_browser_process->GetTabManager()->GetTabCount())
              .Record(ukm::UkmRecorder::Get());
    }
  }

  if (is_in_background_tab_opening_session_) {
    UMA_HISTOGRAM_TIMES(
        kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration,
        queueing_time);

    size_t background_tab_loading_count =
        g_browser_process->GetTabManager()->GetBackgroundTabLoadingCount();
    size_t background_tab_pending_count =
        g_browser_process->GetTabManager()->GetBackgroundTabPendingCount();
    if (ukm_source_id != ukm::kInvalidSourceId &&
        ShouldReportExpectedTaskQueueingDurationToUKM(
            background_tab_loading_count, background_tab_pending_count)) {
      ukm::builders::
          TabManager_BackgroundTabOpening_ForegroundTab_ExpectedTaskQueueingDurationInfo(
              ukm_source_id)
              .SetBackgroundTabLoadingCount(background_tab_loading_count)
              .SetBackgroundTabOpeningSessionId(session_id_)
              .SetBackgroundTabPendingCount(background_tab_pending_count)
              .SetExpectedTaskQueueingDuration(queueing_time.InMilliseconds())
              .SetSequenceId(sequence_++)
              .SetSystemTabCount(
                  g_browser_process->GetTabManager()->GetTabCount())
              .Record(ukm::UkmRecorder::Get());
    }
  }
}

void TabManagerStatsCollector::RecordBackgroundTabCount() {
  DCHECK(is_in_background_tab_opening_session_);

  if (!is_overlapping_background_tab_opening_) {
    UMA_HISTOGRAM_COUNTS_100(kHistogramBackgroundTabOpeningTabCount,
                             background_tab_count_stats_.tab_count);
    UMA_HISTOGRAM_COUNTS_100(kHistogramBackgroundTabOpeningTabPausedCount,
                             background_tab_count_stats_.tab_paused_count);
    UMA_HISTOGRAM_COUNTS_100(
        kHistogramBackgroundTabOpeningTabLoadAutoStartedCount,
        background_tab_count_stats_.tab_load_auto_started_count);
    UMA_HISTOGRAM_COUNTS_100(
        kHistogramBackgroundTabOpeningTabLoadUserInitiatedCount,
        background_tab_count_stats_.tab_load_user_initiated_count);
  }
}

void TabManagerStatsCollector::OnSessionRestoreStartedLoadingTabs() {
  DCHECK(!is_session_restore_loading_tabs_);
  UpdateSessionAndSequence();

  CreateAndInitSwapMetricsDriverIfNeeded(SessionType::kSessionRestore);

  is_session_restore_loading_tabs_ = true;
  ClearStatsWhenInOverlappedSession();
}

void TabManagerStatsCollector::OnSessionRestoreFinishedLoadingTabs() {
  DCHECK(is_session_restore_loading_tabs_);

  UMA_HISTOGRAM_BOOLEAN(kHistogramSessionOverlapSessionRestore,
                        is_overlapping_session_restore_ ? true : false);
  if (swap_metrics_driver_)
    swap_metrics_driver_->UpdateMetrics();

  is_session_restore_loading_tabs_ = false;
  is_overlapping_session_restore_ = false;
}

void TabManagerStatsCollector::OnBackgroundTabOpeningSessionStarted() {
  DCHECK(!is_in_background_tab_opening_session_);
  UpdateSessionAndSequence();
  background_tab_count_stats_.Reset();
  CreateAndInitSwapMetricsDriverIfNeeded(SessionType::kBackgroundTabOpening);

  is_in_background_tab_opening_session_ = true;
  ClearStatsWhenInOverlappedSession();
}

void TabManagerStatsCollector::OnBackgroundTabOpeningSessionEnded() {
  DCHECK(is_in_background_tab_opening_session_);

  UMA_HISTOGRAM_BOOLEAN(kHistogramSessionOverlapBackgroundTabOpening,
                        is_overlapping_background_tab_opening_ ? true : false);
  if (swap_metrics_driver_)
    swap_metrics_driver_->UpdateMetrics();
  RecordBackgroundTabCount();

  is_in_background_tab_opening_session_ = false;
  is_overlapping_background_tab_opening_ = false;
}

void TabManagerStatsCollector::CreateAndInitSwapMetricsDriverIfNeeded(
    SessionType type) {
  if (IsInOverlappedSession()) {
    swap_metrics_driver_ = nullptr;
    return;
  }

  // Always create a new instance in case there is a SessionType change because
  // this is shared between SessionRestore and BackgroundTabOpening.
  swap_metrics_driver_ = content::SwapMetricsDriver::Create(
      base::WrapUnique<content::SwapMetricsDriver::Delegate>(
          new SwapMetricsDelegate(this, type)),
      base::TimeDelta::FromSeconds(0));
  // The driver could still be null on a platform with no swap driver support.
  if (swap_metrics_driver_)
    swap_metrics_driver_->InitializeMetrics();
}

void TabManagerStatsCollector::RecordSwapMetrics(SessionType type,
                                                 const std::string& metric_name,
                                                 uint64_t count,
                                                 base::TimeDelta interval) {
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      "TabManager.Experimental." + std::string(kSessionTypeName[type]) + "." +
          metric_name,
      1,      // minimum
      10000,  // maximum
      50,     // bucket_count
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<double>(count) / interval.InSecondsF());
}

void TabManagerStatsCollector::OnUpdateSwapMetricsFailed() {
  swap_metrics_driver_ = nullptr;
}

void TabManagerStatsCollector::OnDidStartMainFrameNavigation(
    content::WebContents* contents) {
  foreground_contents_switched_to_times_.erase(contents);
}

void TabManagerStatsCollector::OnWillLoadNextBackgroundTab(bool timeout) {
  UMA_HISTOGRAM_BOOLEAN(kHistogramBackgroundTabOpeningTabLoadTimeout, timeout);
}

void TabManagerStatsCollector::OnTabIsLoaded(content::WebContents* contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::Contains(foreground_contents_switched_to_times_, contents))
    return;

  base::TimeDelta switch_load_time =
      NowTicks() - foreground_contents_switched_to_times_[contents];
  ukm::SourceId ukm_source_id = GetUkmSourceId(contents);
  if (is_session_restore_loading_tabs_ && !IsInOverlappedSession()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kHistogramSessionRestoreTabSwitchLoadTime,
                               switch_load_time);

    if (ukm_source_id != ukm::kInvalidSourceId) {
      ukm::builders::
          TabManager_Experimental_SessionRestore_TabSwitchLoadStopped(
              ukm_source_id)
              .SetSequenceId(sequence_++)
              .SetSessionRestoreSessionId(session_id_)
              .SetSessionRestoreTabCount(
                  g_browser_process->GetTabManager()->restored_tab_count())
              .SetSystemTabCount(
                  g_browser_process->GetTabManager()->GetTabCount())
              .SetTabSwitchLoadTime(switch_load_time.InMilliseconds())
              .Record(ukm::UkmRecorder::Get());
    }
  }
  if (is_in_background_tab_opening_session_ && !IsInOverlappedSession()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kHistogramBackgroundTabOpeningTabSwitchLoadTime,
                               switch_load_time);

    if (ukm_source_id != ukm::kInvalidSourceId) {
      ukm::builders::
          TabManager_Experimental_BackgroundTabOpening_TabSwitchLoadStopped(
              ukm_source_id)
              .SetBackgroundTabLoadingCount(
                  g_browser_process->GetTabManager()
                      ->GetBackgroundTabLoadingCount())
              .SetBackgroundTabOpeningSessionId(session_id_)
              .SetBackgroundTabPendingCount(
                  g_browser_process->GetTabManager()
                      ->GetBackgroundTabPendingCount())
              .SetSequenceId(sequence_++)
              .SetSystemTabCount(
                  g_browser_process->GetTabManager()->GetTabCount())
              .SetTabSwitchLoadTime(switch_load_time.InMilliseconds())
              .Record(ukm::UkmRecorder::Get());
    }
  }

  foreground_contents_switched_to_times_.erase(contents);
}

void TabManagerStatsCollector::OnWebContentsDestroyed(
    content::WebContents* contents) {
  foreground_contents_switched_to_times_.erase(contents);
}

bool TabManagerStatsCollector::IsInOverlappedSession() {
  return is_session_restore_loading_tabs_ &&
         is_in_background_tab_opening_session_;
}

void TabManagerStatsCollector::ClearStatsWhenInOverlappedSession() {
  if (!IsInOverlappedSession())
    return;

  swap_metrics_driver_ = nullptr;
  foreground_contents_switched_to_times_.clear();
  background_tab_count_stats_.Reset();

  is_overlapping_session_restore_ = true;
  is_overlapping_background_tab_opening_ = true;
}

void TabManagerStatsCollector::UpdateSessionAndSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function is used by both SessionRestore and BackgroundTabOpening. This
  // is fine because we do not report any metric when those two overlap.
  ++session_id_;
  sequence_ = 0;
}

void TabManagerStatsCollector::StartPeriodicSampling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Post a first task with a random delay less than the sampling interval.
  base::TimeDelta delay = base::TimeDelta::FromSeconds(
      base::RandInt(0, kLowFrequencySamplingInterval.InSeconds()));
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabManagerStatsCollector::PerformPeriodicSample,
                     weak_factory_.GetWeakPtr()),
      delay);
}

void TabManagerStatsCollector::PerformPeriodicSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sample_start_time_ = NowTicks();

  // Iterate over the tabs and get their data. The TabManager owns us and
  // outlives us, so will always exist.
  LifecycleUnitVector lifecycle_units =
      g_browser_process->GetTabManager()->GetSortedLifecycleUnits();
  for (auto* lifecycle_unit : lifecycle_units) {
    DecisionDetails freeze_decision;
    lifecycle_unit->CanFreeze(&freeze_decision);
    RecordDecisionDetails(lifecycle_unit, freeze_decision,
                          LifecycleUnitState::FROZEN);

    DecisionDetails discard_decision;
    lifecycle_unit->CanDiscard(LifecycleUnitDiscardReason::PROACTIVE,
                               &discard_decision);
    RecordDecisionDetails(lifecycle_unit, discard_decision,
                          LifecycleUnitState::DISCARDED);
  }

  // Determine when the next sample should run based on when this cycle
  // started.
  base::TimeDelta delay =
      (sample_start_time_ + kLowFrequencySamplingInterval) - NowTicks();

  // In the very unlikely case that the system is so busy that another sample
  // should already have been taken, then skip a cycle and wait a full sampling
  // period. This provides rudimentary rate limiting that prevents these samples
  // from taking up too much time.
  if (delay <= base::TimeDelta())
    delay = kLowFrequencySamplingInterval;

  // Schedule the next sample.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabManagerStatsCollector::PerformPeriodicSample,
                     weak_factory_.GetWeakPtr()),
      delay);
}

// static
void TabManagerStatsCollector::RecordDecisionDetails(
    LifecycleUnit* lifecycle_unit,
    const DecisionDetails& decision_details,
    LifecycleUnitState target_state) {
  ukm::SourceId ukm_source_id = lifecycle_unit->GetUkmSourceId();
  if (ukm_source_id == ukm::kInvalidSourceId)
    return;

  // Don't log anything for invalid decision details (trivial reasons: crashed
  // tabs, navigations not yet committed, etc).
  if (decision_details.reasons().empty())
    return;

  ukm::builders::TabManager_LifecycleStateChange builder(ukm_source_id);

  builder.SetOldLifecycleState(
      static_cast<int64_t>(lifecycle_unit->GetState()));
  builder.SetNewLifecycleState(static_cast<int64_t>(target_state));
  // No LifecycleStateChangeReason is set right now, indicating that this is a
  // theoretical state change rather than an actual one. This differentiates
  // sampled lifecycle transitions from actual ones.

  // We only currently report transitions for tabs, so this lookup should never
  // fail. It will start failing once we add ARC processes as LifecycleUnits.
  // TODO(chrisha): This should be time since the navigation was committed (the
  // load started), but that information is currently only persisted inside the
  // CU-graph. Using time since navigation finished is a cheap approximation for
  // the time being.
  auto* tab = lifecycle_unit->AsTabLifecycleUnitExternal();
  auto* contents = tab->GetWebContents();
  auto* nav_entry = contents->GetController().GetLastCommittedEntry();
  if (nav_entry) {
    auto timestamp = nav_entry->GetTimestamp();
    if (!timestamp.is_null()) {
      auto elapsed = base::Time::Now() - timestamp;
      builder.SetTimeSinceNavigationMs(elapsed.InMilliseconds());
    }
  }

  // Set visibility related data.
  // |time_since_visible| is:
  // - Zero if the LifecycleUnit is currently visible.
  // - Time since creation if the LifecycleUnit was never visible.
  // - Time since visible if the LifecycleUnit was visible in the past.
  auto visibility = lifecycle_unit->GetVisibility();
  base::TimeDelta time_since_visible;  // Zero.
  if (visibility != content::Visibility::VISIBLE)
    time_since_visible = NowTicks() - lifecycle_unit->GetWallTimeWhenHidden();
  builder.SetTimeSinceVisibilityStateChangeMs(
      time_since_visible.InMilliseconds());
  builder.SetVisibilityState(static_cast<int64_t>(visibility));

  // This populates all of the relevant Success/Failure fields, as well as
  // Outcome.
  decision_details.Populate(&builder);

  builder.Record(ukm::UkmRecorder::Get());
}

// static
const char TabManagerStatsCollector::
    kHistogramSessionRestoreForegroundTabExpectedTaskQueueingDuration[] =
        "TabManager.SessionRestore.ForegroundTab.ExpectedTaskQueueingDuration";

// static
const char TabManagerStatsCollector::
    kHistogramBackgroundTabOpeningForegroundTabExpectedTaskQueueingDuration[] =
        "TabManager.BackgroundTabOpening.ForegroundTab."
        "ExpectedTaskQueueingDuration";

// static
const char TabManagerStatsCollector::kHistogramSessionRestoreSwitchToTab[] =
    "TabManager.SessionRestore.SwitchToTab";

// static
const char
    TabManagerStatsCollector::kHistogramBackgroundTabOpeningSwitchToTab[] =
        "TabManager.BackgroundTabOpening.SwitchToTab";

// static
const char
    TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime[] =
        "TabManager.Experimental.SessionRestore.TabSwitchLoadTime."
        "UntilTabIsLoaded";

// static
const char TabManagerStatsCollector::
    kHistogramBackgroundTabOpeningTabSwitchLoadTime[] =
        "TabManager.Experimental.BackgroundTabOpening.TabSwitchLoadTime."
        "UntilTabIsLoaded";

// static
const char TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabCount[] =
    "TabManager.BackgroundTabOpening.TabCount";

// static
const char
    TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabPausedCount[] =
        "TabManager.BackgroundTabOpening.TabPausedCount";

// static
const char TabManagerStatsCollector::
    kHistogramBackgroundTabOpeningTabLoadAutoStartedCount[] =
        "TabManager.BackgroundTabOpening.TabLoadAutoStartedCount";

// static
const char TabManagerStatsCollector::
    kHistogramBackgroundTabOpeningTabLoadUserInitiatedCount[] =
        "TabManager.BackgroundTabOpening.TabLoadUserInitiatedCount";

// static
const char
    TabManagerStatsCollector::kHistogramBackgroundTabOpeningTabLoadTimeout[] =
        "TabManager.BackgroundTabOpening.TabLoadTimeout";

// static
const char TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore[] =
    "TabManager.SessionOverlap.SessionRestore";

// static
const char
    TabManagerStatsCollector::kHistogramSessionOverlapBackgroundTabOpening[] =
        "TabManager.SessionOverlap.BackgroundTabOpening";

// static
constexpr base::TimeDelta
    TabManagerStatsCollector::kLowFrequencySamplingInterval;

}  // namespace resource_coordinator

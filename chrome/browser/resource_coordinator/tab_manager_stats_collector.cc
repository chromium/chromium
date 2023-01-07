// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
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
#include "content/public/browser/swap_metrics_driver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

ukm::SourceId GetUkmSourceId(content::WebContents* contents) {
  resource_coordinator::ResourceCoordinatorTabHelper* observer =
      resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          contents);
  if (!observer)
    return ukm::kInvalidSourceId;

  return observer->ukm_source_id();
}

}  // namespace

class TabManagerStatsCollector::SwapMetricsDelegate
    : public content::SwapMetricsDriver::Delegate {
 public:
  explicit SwapMetricsDelegate(
      TabManagerStatsCollector* tab_manager_stats_collector)
      : tab_manager_stats_collector_(tab_manager_stats_collector) {}

  ~SwapMetricsDelegate() override = default;

  void OnSwapInCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics("SwapInPerSecond", count,
                                                    interval);
  }

  void OnSwapOutCount(uint64_t count, base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics("SwapOutPerSecond", count,
                                                    interval);
  }

  void OnDecompressedPageCount(uint64_t count,
                               base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics(
        "DecompressedPagesPerSecond", count, interval);
  }

  void OnCompressedPageCount(uint64_t count,
                             base::TimeDelta interval) override {
    tab_manager_stats_collector_->RecordSwapMetrics("CompressedPagesPerSecond",
                                                    count, interval);
  }

  void OnUpdateMetricsFailed() override {
    tab_manager_stats_collector_->OnUpdateSwapMetricsFailed();
  }

 private:
  raw_ptr<TabManagerStatsCollector> tab_manager_stats_collector_;
};

TabManagerStatsCollector::TabManagerStatsCollector() {
  SessionRestore::AddObserver(this);
}

TabManagerStatsCollector::~TabManagerStatsCollector() {
  SessionRestore::RemoveObserver(this);
}

void TabManagerStatsCollector::RecordSwitchToTab(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (!is_session_restore_loading_tabs_) {
    return;
  }

  auto* new_data = TabManager::WebContentsData::FromWebContents(new_contents);
  DCHECK(new_data);

  if (is_session_restore_loading_tabs_) {
    UMA_HISTOGRAM_ENUMERATION(
        kHistogramSessionRestoreSwitchToTab,
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

void TabManagerStatsCollector::OnSessionRestoreStartedLoadingTabs() {
  DCHECK(!is_session_restore_loading_tabs_);
  UpdateSessionAndSequence();

  CreateAndInitSwapMetricsDriverIfNeeded();

  is_session_restore_loading_tabs_ = true;
}

void TabManagerStatsCollector::OnSessionRestoreFinishedLoadingTabs() {
  DCHECK(is_session_restore_loading_tabs_);

  UMA_HISTOGRAM_BOOLEAN(kHistogramSessionOverlapSessionRestore, false);
  if (swap_metrics_driver_)
    swap_metrics_driver_->UpdateMetrics();

  is_session_restore_loading_tabs_ = false;
}

void TabManagerStatsCollector::CreateAndInitSwapMetricsDriverIfNeeded() {
  swap_metrics_driver_ = content::SwapMetricsDriver::Create(
      base::WrapUnique<content::SwapMetricsDriver::Delegate>(
          new SwapMetricsDelegate(this)),
      base::Seconds(0));
  // The driver could still be null on a platform with no swap driver support.
  if (swap_metrics_driver_)
    swap_metrics_driver_->InitializeMetrics();
}

void TabManagerStatsCollector::RecordSwapMetrics(const std::string& metric_name,
                                                 uint64_t count,
                                                 base::TimeDelta interval) {
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      "TabManager.Experimental.SessionRestore." + metric_name,
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

void TabManagerStatsCollector::OnTabIsLoaded(content::WebContents* contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::Contains(foreground_contents_switched_to_times_, contents))
    return;

  base::TimeDelta switch_load_time =
      NowTicks() - foreground_contents_switched_to_times_[contents];
  ukm::SourceId ukm_source_id = GetUkmSourceId(contents);
  if (is_session_restore_loading_tabs_) {
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

  foreground_contents_switched_to_times_.erase(contents);
}

void TabManagerStatsCollector::OnWebContentsDestroyed(
    content::WebContents* contents) {
  foreground_contents_switched_to_times_.erase(contents);
}

void TabManagerStatsCollector::UpdateSessionAndSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ++session_id_;
  sequence_ = 0;
}

// static
const char TabManagerStatsCollector::kHistogramSessionRestoreSwitchToTab[] =
    "TabManager.SessionRestore.SwitchToTab";

// static
const char
    TabManagerStatsCollector::kHistogramSessionRestoreTabSwitchLoadTime[] =
        "TabManager.Experimental.SessionRestore.TabSwitchLoadTime."
        "UntilTabIsLoaded";

// static
const char TabManagerStatsCollector::kHistogramSessionOverlapSessionRestore[] =
    "TabManager.SessionOverlap.SessionRestore";

// static
constexpr base::TimeDelta
    TabManagerStatsCollector::kLowFrequencySamplingInterval;

}  // namespace resource_coordinator

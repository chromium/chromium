// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace performance_manager::metrics {

namespace {

using PageMeasurementBackgroundState =
    PageTimelineMonitor::PageMeasurementBackgroundState;

using PageCPUUsageVector = std::vector<std::pair<const PageNode*, double>>;

// CPU usage metrics are provided as a double in the [0.0, number of cores *
// 100.0] range. The CPU usage is usually below 1%, so the UKM is
// reported out of 10,000 instead of out of 100 to make analyzing the data
// easier. This is the same scale factor used by the
// PerformanceMonitor.AverageCPU8 histograms recorded in
// chrome/browser/metrics/power/process_metrics_recorder_util.cc.
constexpr int kCPUUsageFactor = 100 * 100;

PageMeasurementBackgroundState GetBackgroundStateForMeasurementPeriod(
    const PageNode* page_node,
    base::TimeDelta time_since_last_measurement) {
  if (page_node->GetTimeSinceLastVisibilityChange() <
      time_since_last_measurement) {
    return PageMeasurementBackgroundState::kMixedForegroundBackground;
  }
  if (page_node->IsVisible()) {
    return PageMeasurementBackgroundState::kForeground;
  }
  // Check if the page was audible for the entire measurement period.
  if (page_node->GetTimeSinceLastAudibleChange().value_or(
          base::TimeDelta::Max()) < time_since_last_measurement) {
    return PageMeasurementBackgroundState::kBackgroundMixedAudible;
  }
  if (page_node->IsAudible()) {
    return PageMeasurementBackgroundState::kAudibleInBackground;
  }
  return PageMeasurementBackgroundState::kBackground;
}

}  // namespace

PageTimelineMonitor::PageTimelineMonitor()
    // These counters are initialized to a random value due to privacy concerns,
    // so that we cannot tie either the startup time of a specific tab or the
    // recording time of a specific slice to the browser startup time.
    : slice_id_counter_(base::RandInt(1, 32767)) {
  collect_slice_timer_.Start(
      FROM_HERE,
      performance_manager::features::kPageTimelineStateIntervalTime.Get(), this,
      &PageTimelineMonitor::CollectSlice);

  // PageResourceUsage is collected on a different schedule from PageTimeline.
  collect_page_resource_usage_timer_.Start(
      FROM_HERE, base::Minutes(2), this,
      &PageTimelineMonitor::CollectPageResourceUsage);
}

PageTimelineMonitor::~PageTimelineMonitor() = default;

PageTimelineMonitor::PageState
PageTimelineMonitor::PageNodeInfo::GetPageState() {
  switch (current_lifecycle) {
    case PageNode::LifecycleState::kRunning: {
      if (currently_visible) {
        return PageState::kVisible;
      } else {
        return PageState::kBackground;
      }
    }
    case PageNode::LifecycleState::kFrozen: {
      return PageState::kFrozen;
    }
    case PageNode::LifecycleState::kDiscarded: {
      return PageState::kDiscarded;
    }
  }
}

void PageTimelineMonitor::CollectPageResourceUsage() {
  // Calculate the overall CPU usage.
  double total_cpu_usage = 0;
  const PageCPUUsageVector page_cpu_usage = CalculatePageCPUUsage();
  for (const auto& [page_node, cpu_usage] : page_cpu_usage) {
    total_cpu_usage += cpu_usage;
  }

  const auto now = base::TimeTicks::Now();
  for (const auto& [page_node, cpu_usage] : page_cpu_usage) {
    const ukm::SourceId source_id = page_node->GetUkmSourceID();
    ukm::builders::PerformanceManager_PageResourceUsage2(source_id)
        .SetResidentSetSizeEstimate(page_node->EstimateResidentSetSize())
        .SetPrivateFootprintEstimate(page_node->EstimatePrivateFootprintSize())
        .SetRecentCPUUsage(kCPUUsageFactor * cpu_usage)
        .SetTotalRecentCPUUsageAllPages(kCPUUsageFactor * total_cpu_usage)
        .SetBackgroundState(
            static_cast<int64_t>(GetBackgroundStateForMeasurementPeriod(
                page_node, now - time_of_last_resource_usage_)))
        .Record(ukm::UkmRecorder::Get());
  }
  time_of_last_resource_usage_ = now;

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kCPUInterventionEvaluationLogging)) {
    bool is_cpu_over_threshold =
        (100 * total_cpu_usage / base::SysInfo::NumberOfProcessors() >
         performance_manager::features::kThresholdChromeCPUPercent.Get());
    if (!time_of_last_cpu_threshold_exceeded_.has_value()) {
      CHECK(!log_cpu_on_delay_timer_.IsRunning());
      if (is_cpu_over_threshold) {
        time_of_last_cpu_threshold_exceeded_ = now;
        LogCPUInterventionMetrics(page_cpu_usage, now, "Immediate");

        // Only logged delayed metrics when using the new CPU monitor.
        if (performance_manager::features::kUseResourceAttributionCPUMonitor
                .Get()) {
          log_cpu_on_delay_timer_.Start(
              FROM_HERE,
              performance_manager::features::kDelayBeforeLogging.Get(), this,
              &PageTimelineMonitor::CheckDelayedCPUInterventionMetrics);
        }
      }
    } else if (!is_cpu_over_threshold) {
      base::UmaHistogramCustomTimes(
          "PerformanceManager.PerformanceInterventions.CPU."
          "DurationOverThreshold",
          now - time_of_last_cpu_threshold_exceeded_.value(), base::Minutes(2),
          base::Hours(24), 50);
      log_cpu_on_delay_timer_.AbandonAndStop();
      time_of_last_cpu_threshold_exceeded_ = absl::nullopt;
    }
  }
#endif
}

void PageTimelineMonitor::CollectSlice() {
  // We only collect a slice randomly every ~20 times this gets called for
  // privacy purposes. Always fall through when we're in a test.
  if (!ShouldCollectSlice()) {
    return;
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  const int slice_id = slice_id_counter_++;
  base::TimeDelta time_since_last_slice = now - time_of_last_slice_;

  time_of_last_slice_ = now;

  for (auto const& pair : page_node_info_map_) {
    const PageNode* page_node = pair.first->page_node();
    const std::unique_ptr<PageNodeInfo>& curr_info = pair.second;
    CheckPageState(page_node, *curr_info);

    const PageNode::LifecycleState lifecycle_state =
        page_node->GetLifecycleState();
    const bool is_visible = page_node->IsVisible();
    const ukm::SourceId source_id = page_node->GetUkmSourceID();

    DCHECK_EQ(is_visible, curr_info->currently_visible);
    DCHECK(curr_info->current_lifecycle == mojom::LifecycleState::kDiscarded ||
           lifecycle_state == curr_info->current_lifecycle);

    if (is_visible) {
      curr_info->total_foreground_milliseconds +=
          (now - curr_info->time_of_last_foreground_millisecond_update)
              .InMilliseconds();
      curr_info->time_of_last_foreground_millisecond_update = now;
    }

    bool is_active_tab = false;
    bool has_notification_permission = false;
    bool is_capturing_media = false;
    bool is_connected_to_device = false;
    bool updated_title_or_favicon_in_background = false;

    const auto* page_live_state_data =
        PageLiveStateDecorator::Data::FromPageNode(page_node);
    if (page_live_state_data) {
      is_active_tab = page_live_state_data->IsActiveTab();
      has_notification_permission =
          page_live_state_data->IsContentSettingTypeAllowed(
              ContentSettingsType::NOTIFICATIONS);
      is_capturing_media = page_live_state_data->IsCapturingVideo() ||
                           page_live_state_data->IsCapturingAudio() ||
                           page_live_state_data->IsBeingMirrored() ||
                           page_live_state_data->IsCapturingWindow() ||
                           page_live_state_data->IsCapturingDisplay();
      is_connected_to_device =
          page_live_state_data->IsConnectedToUSBDevice() ||
          page_live_state_data->IsConnectedToBluetoothDevice();
      updated_title_or_favicon_in_background =
          page_live_state_data->UpdatedTitleOrFaviconInBackground();
    }

    ukm::builders::PerformanceManager_PageTimelineState builder(source_id);

    builder.SetSliceId(slice_id)
        .SetIsActiveTab(is_active_tab)
        .SetTimeSinceLastSlice(ukm::GetSemanticBucketMinForDurationTiming(
            time_since_last_slice.InMilliseconds()))
        .SetTimeSinceCreation(ukm::GetSemanticBucketMinForDurationTiming(
            (now - curr_info->time_of_creation).InMilliseconds()))
        .SetCurrentState(static_cast<uint64_t>(curr_info->GetPageState()))
        .SetTimeInCurrentState(ukm::GetSemanticBucketMinForDurationTiming(
            (now - curr_info->time_of_most_recent_state_change)
                .InMilliseconds()))
        .SetTotalForegroundTime(ukm::GetSemanticBucketMinForDurationTiming(
            curr_info->total_foreground_milliseconds))
        .SetChangedFaviconOrTitleInBackground(
            updated_title_or_favicon_in_background)
        .SetHasNotificationPermission(has_notification_permission)
        .SetIsCapturingMedia(is_capturing_media)
        .SetIsConnectedToDevice(is_connected_to_device)
        .SetIsPlayingAudio(page_node->IsAudible())
        .SetPrivateFootprint(page_node->EstimatePrivateFootprintSize())
        .SetResidentSetSize(page_node->EstimateResidentSetSize())
        .SetTabId(curr_info->tab_id);

#if !BUILDFLAG(IS_ANDROID)
    bool high_efficiency_mode_active =
        (policies::HighEfficiencyModePolicy::GetInstance() &&
         policies::HighEfficiencyModePolicy::GetInstance()
             ->IsHighEfficiencyDiscardingEnabled()) ||
        (policies::HeuristicMemorySaverPolicy::GetInstance() &&
         policies::HeuristicMemorySaverPolicy::GetInstance()->IsActive());

    builder.SetHighEfficiencyMode(high_efficiency_mode_active)
        .SetBatterySaverMode(battery_saver_enabled_);
#endif  // !BUILDFLAG(IS_ANDROID)

    builder.Record(ukm::UkmRecorder::Get());
  }
}

bool PageTimelineMonitor::ShouldCollectSlice() const {
  if (should_collect_slice_callback_) {
    return should_collect_slice_callback_.Run();
  }

  // The default if not overridden by tests is to report ~1 out of 20 slices.
  return base::RandInt(0, 19) == 1;
}

void PageTimelineMonitor::CheckDelayedCPUInterventionMetrics() {
  CHECK(performance_manager::features::kUseResourceAttributionCPUMonitor.Get());

  double total_cpu_usage = 0;
  auto page_cpu_usage = CalculatePageCPUUsage();
  for (const auto& [page_node, cpu_usage] : page_cpu_usage) {
    total_cpu_usage += cpu_usage;
  }

  if (100 * total_cpu_usage / base::SysInfo::NumberOfProcessors() >
      performance_manager::features::kThresholdChromeCPUPercent.Get()) {
    // Still over the threshold so we should log .Delayed UMA metrics.
    LogCPUInterventionMetrics(page_cpu_usage, base::TimeTicks::Now(),
                              "Delayed");
  }
}

void PageTimelineMonitor::LogCPUInterventionMetrics(
    const PageCPUUsageVector page_cpu_usage,
    const base::TimeTicks now,
    const std::string& suffix) {
  double total_background_cpu_usage = 0;

  for (const auto& [page_node, cpu_usage] : page_cpu_usage) {
    if (GetBackgroundStateForMeasurementPeriod(
            page_node, now - time_of_last_resource_usage_) !=
        PageMeasurementBackgroundState::kForeground) {
      total_background_cpu_usage += cpu_usage;
    }
  }

  base::UmaHistogramPercentage(
      "PerformanceManager.PerformanceInterventions.CPU."
      "TotalBackgroundCPU." +
          suffix,
      total_background_cpu_usage * 100 / base::SysInfo::NumberOfProcessors());
}

PageCPUUsageVector PageTimelineMonitor::CalculatePageCPUUsage() {
  const PageTimelineCPUMonitor::CPUUsageMap cpu_usage_map =
      cpu_monitor_.UpdateCPUMeasurements();

  // Calculate the overall CPU usage.
  PageCPUUsageVector page_cpu_usage;
  page_cpu_usage.reserve(page_node_info_map_.size());

  for (const auto& [tab_handle, info_ptr] : page_node_info_map_) {
    const PageNode* page_node = tab_handle->page_node();
    CheckPageState(page_node, *info_ptr);
    double cpu_usage =
        PageTimelineCPUMonitor::EstimatePageCPUUsage(page_node, cpu_usage_map);
    page_cpu_usage.emplace_back(page_node, cpu_usage);
  }

  return page_cpu_usage;
}

void PageTimelineMonitor::SetTriggerCollectionManuallyForTesting() {
  collect_slice_timer_.Stop();
  collect_page_resource_usage_timer_.Stop();
  log_cpu_on_delay_timer_.Stop();
}

void PageTimelineMonitor::SetShouldCollectSliceCallbackForTesting(
    base::RepeatingCallback<bool()> should_collect_slice_callback) {
  should_collect_slice_callback_ = should_collect_slice_callback;
}

void PageTimelineMonitor::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  graph_->AddPageNodeObserver(this);
  graph_->RegisterObject(this);
  graph->GetRegisteredObjectAs<TabPageDecorator>()->AddObserver(this);
  cpu_monitor_.StartMonitoring(graph_);
}

void PageTimelineMonitor::OnTakenFromGraph(Graph* graph) {
  cpu_monitor_.StopMonitoring(graph_);

  // GraphOwned object destruction order is undefined, so only remove ourselves
  // as observers if the decorator still exists.
  TabPageDecorator* tab_page_decorator =
      graph->GetRegisteredObjectAs<TabPageDecorator>();
  if (tab_page_decorator) {
    tab_page_decorator->RemoveObserver(this);
  }

  graph_->UnregisterObject(this);
  graph_->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void PageTimelineMonitor::OnTabAdded(TabPageDecorator::TabHandle* tab_handle) {
  page_node_info_map_[tab_handle] = std::make_unique<PageNodeInfo>(
      base::TimeTicks::Now(), tab_handle->page_node(), slice_id_counter_++);
}

void PageTimelineMonitor::OnTabAboutToBeDiscarded(
    const PageNode* old_page_node,
    TabPageDecorator::TabHandle* tab_handle) {
  auto it = page_node_info_map_.find(tab_handle);
  CHECK(it != page_node_info_map_.end());

  it->second->current_lifecycle = mojom::LifecycleState::kDiscarded;
  CheckPageState(tab_handle->page_node(), *it->second);
}

void PageTimelineMonitor::OnBeforeTabRemoved(
    TabPageDecorator::TabHandle* tab_handle) {
  page_node_info_map_.erase(tab_handle);
}

void PageTimelineMonitor::OnIsVisibleChanged(const PageNode* page_node) {
  if (page_node->GetType() != performance_manager::PageType::kTab) {
    return;
  }

  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(page_node);
  // It's possible for this to happen when a tab is discarded. The sequence of
  // events is:
  // 1. New web contents (and page node) created
  // 2. AboutToBeDiscarded(old_page_node, new_page_node) is invoked
  // 3. Tab is detached from the tabstrip, causing its web contents to become
  // "occluded", which triggers a visibility change notification
  // 4. The old web contents (and page node) are deleted
  // In the case of PageTimelineMonitor, the page_node is removed from the map
  // on step 2, so the notification from step 3 has to be ignored.
  if (!tab_handle) {
    return;
  }

  auto it = page_node_info_map_.find(tab_handle);
  CHECK(it != page_node_info_map_.end());

  std::unique_ptr<PageNodeInfo>& info = it->second;
  base::TimeTicks now = base::TimeTicks::Now();
  if (info->currently_visible && !page_node->IsVisible()) {
    // Increase total foreground seconds by the time since we entered the
    // foreground now that we are entering the background.
    info->total_foreground_milliseconds +=
        (now - info->time_of_last_foreground_millisecond_update)
            .InMilliseconds();
    info->time_of_last_foreground_millisecond_update = now;
  } else if (!info->currently_visible && page_node->IsVisible()) {
    // Update time_of_last[...] without increasing
    // total_foreground_milliseconds because we've just entered the
    // foreground.
    info->time_of_last_foreground_millisecond_update = now;
  }
  info->currently_visible = page_node->IsVisible();
  info->time_of_most_recent_state_change = now;
}

void PageTimelineMonitor::OnPageLifecycleStateChanged(
    const PageNode* page_node) {
  if (page_node->GetType() != performance_manager::PageType::kTab) {
    return;
  }

  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(page_node);
  if (!tab_handle) {
    // This function is called by the tab freezing apparatus between the time a
    // page is discarded and when its PageNode is removed from the graph. In
    // that situation, it's not in the map anymore, it doesn't have a tab
    // handle, and another PageNode is being tracked in its place. It's safe to
    // return early.
    return;
  }

  auto it = page_node_info_map_.find(tab_handle);
  CHECK(it != page_node_info_map_.end());

  it->second->current_lifecycle = page_node->GetLifecycleState();
  it->second->time_of_most_recent_state_change = base::TimeTicks::Now();
}

void PageTimelineMonitor::SetBatterySaverEnabled(bool enabled) {
  battery_saver_enabled_ = enabled;
}

void PageTimelineMonitor::CheckPageState(const PageNode* page_node,
                                         const PageNodeInfo& info) {
  // There's a window after OnAboutToBeDiscarded() where a discarded placeholder
  // page is in the map with type kUnknown, before it's updated to kTab in
  // OnTypeChanged().
  CHECK(page_node->GetType() == PageType::kTab ||
        page_node->GetType() == PageType::kUnknown &&
            info.current_lifecycle == mojom::LifecycleState::kDiscarded);
}

}  // namespace performance_manager::metrics

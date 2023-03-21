// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_timeline_monitor.h"

#include <stdint.h>
#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/performance_manager/policies/heuristic_memory_saver_policy.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace performance_manager::metrics {

PageTimelineMonitor::PageTimelineMonitor()
    // These counters are initialized to a random value due to privacy concerns,
    // so that we cannot tie either the startup time of a specific tab or the
    // recording time of a specific slice to the browser startup time.
    : slice_id_counter_(base::RandInt(1, 32767)) {
  collect_slice_timer_.Start(
      FROM_HERE,
      performance_manager::features::kPageTimelineStateIntervalTime.Get(), this,
      &PageTimelineMonitor::CollectSlice);
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

void PageTimelineMonitor::CollectSlice() {
  // Whether or not we record a full PageTimelineState slice, record the
  // estimated memory usage, which has fewer privacy implications so can be
  // recorded more often.
  for (auto const& pair : page_node_info_map_) {
    const PageNode* page_node = pair.first;

    DCHECK_EQ(page_node->GetType(), performance_manager::PageType::kTab);
    const ukm::SourceId source_id = page_node->GetUkmSourceID();

    ukm::builders::PerformanceManager_PageResourceUsage(source_id)
        .SetResidentSetSizeEstimate(page_node->EstimateResidentSetSize())
        .SetPrivateFootprintEstimate(page_node->EstimatePrivateFootprintSize())
        .Record(ukm::UkmRecorder::Get());
  }

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
    const PageNode* page_node = pair.first;
    const std::unique_ptr<PageNodeInfo>& curr_info = pair.second;

    DCHECK_EQ(page_node->GetType(), performance_manager::PageType::kTab);

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

void PageTimelineMonitor::SetShouldCollectSliceCallbackForTesting(
    base::RepeatingCallback<bool()> should_collect_slice_callback) {
  should_collect_slice_callback_ = should_collect_slice_callback;
}

void PageTimelineMonitor::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  graph_->AddPageNodeObserver(this);
  graph_->RegisterObject(this);
}

void PageTimelineMonitor::OnTakenFromGraph(Graph* graph) {
  graph_->UnregisterObject(this);
  graph_->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void PageTimelineMonitor::OnPageNodeAdded(const PageNode* page_node) {
  DCHECK_EQ(page_node->GetType(), performance_manager::PageType::kUnknown);
}

void PageTimelineMonitor::OnBeforePageNodeRemoved(const PageNode* page_node) {
  // This is a no-op if the pointer is not in the map, so no conditional erase.
  page_node_info_map_.erase(page_node);
}

void PageTimelineMonitor::OnIsVisibleChanged(const PageNode* page_node) {
  if (page_node->GetType() != performance_manager::PageType::kTab)
    return;

  // It's possible for this to happen when a tab is discarded. The sequence of
  // events is:
  // 1. New web contents (and page node) created
  // 2. AboutToBeDiscarded(old_page_node, new_page_node) is invoked
  // 3. Tab is detached from the tabstrip, causing its web contents to become
  // "occluded", which triggers a visibility change notification
  // 4. The old web contents (and page node) are deleted
  // In the case of PageTimelineMonitor, the page_node is removed from the map
  // on step 2, so the notification from step 3 has to be ignored.
  if (!base::Contains(page_node_info_map_, page_node)) {
    return;
  }

  std::unique_ptr<PageNodeInfo>& info = page_node_info_map_[page_node];
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
  if (page_node->GetType() != performance_manager::PageType::kTab)
    return;

  auto it = page_node_info_map_.find(page_node);
  if (it == page_node_info_map_.end()) {
    // This function is called by the tab freezing apparatus between the time a
    // page is discarded and when its PageNode is removed from the graph. In
    // that situation, it's not in the map anymore, and another PageNode is
    // being tracked in its place. It's safe to return early.
    return;
  }

  it->second->current_lifecycle = page_node->GetLifecycleState();
  it->second->time_of_most_recent_state_change = base::TimeTicks::Now();
}

void PageTimelineMonitor::OnTypeChanged(const PageNode* page_node,
                                        PageType previous_state) {
  // If a PageNode already has a PageNodeInfo, its only valid state is
  // `kDiscarded` and a new PageNodeInfo shouldn't be created for it.
  if (base::Contains(page_node_info_map_, page_node)) {
    DCHECK_EQ(page_node_info_map_[page_node]->current_lifecycle,
              mojom::LifecycleState::kDiscarded);
    return;
  }

  // When PageNodes are added, they have type kUnknown, and so it is when new
  // nodes get changed to being of type kTab that we can start using them.
  switch (page_node->GetType()) {
    case performance_manager::PageType::kTab:
      page_node_info_map_[page_node] = std::make_unique<PageNodeInfo>(
          base::TimeTicks::Now(), page_node, slice_id_counter_++);
      break;
    case performance_manager::PageType::kExtension:
      // We won't be dealing with these because we're not recording this UKM
      // for extensions.
      break;
    case performance_manager::PageType::kUnknown:
      NOTREACHED();
      break;
  }
}

void PageTimelineMonitor::OnAboutToBeDiscarded(const PageNode* page_node,
                                               const PageNode* new_page_node) {
  auto old_it = page_node_info_map_.find(page_node);
  DCHECK(old_it != page_node_info_map_.end());
  old_it->second->current_lifecycle = mojom::LifecycleState::kDiscarded;

  bool inserted =
      page_node_info_map_.emplace(new_page_node, std::move(old_it->second))
          .second;
  DCHECK(inserted);

  page_node_info_map_.erase(old_it);
}

void PageTimelineMonitor::SetBatterySaverEnabled(bool enabled) {
  battery_saver_enabled_ = enabled;
}

}  // namespace performance_manager::metrics

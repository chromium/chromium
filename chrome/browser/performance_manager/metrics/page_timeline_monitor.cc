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
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace performance_manager::metrics {

PageTimelineMonitor::PageTimelineMonitor()
    : PageTimelineMonitor(
          base::BindRepeating([]() { return base::RandInt(0, 19) == 1; })) {}

PageTimelineMonitor::PageTimelineMonitor(
    base::RepeatingCallback<bool()> should_collect_slice_callback)
    // These counters are initialized to a random value due to privacy concerns,
    // so that we cannot tie either the startup time of a specific tab or the
    // recording time of a specific slice to the browser startup time.
    : slice_id_counter_(base::RandInt(1, 32767)),
      should_collect_slice_callback_(std::move(should_collect_slice_callback)) {
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
  // We only collect a slice randomly every ~20 times this gets called for
  // privacy purposes. Always fall through when we're in a test.
  if (!should_collect_slice_callback_.Run()) {
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
    DCHECK_EQ(lifecycle_state, curr_info->current_lifecycle);

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
    }

    ukm::builders::PerformanceManager_PageTimelineState(source_id)
        .SetSliceId(slice_id)
#if !BUILDFLAG(IS_ANDROID)
        .SetHighEfficiencyMode(performance_manager::policies::
                                   HighEfficiencyModePolicy::GetInstance() &&
                               performance_manager::policies::
                                   HighEfficiencyModePolicy::GetInstance()
                                       ->IsHighEfficiencyDiscardingEnabled())
        .SetBatterySaverMode(battery_saver_enabled_)
#endif  // !BUILDFLAG(IS_ANDROID)
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
            curr_info->updated_title_or_favicon_in_background)
        .SetHasNotificationPermission(has_notification_permission)
        .SetIsCapturingMedia(is_capturing_media)
        .SetIsConnectedToDevice(is_connected_to_device)
        .SetIsPlayingAudio(page_node->IsAudible())
        .SetResidentSetSize(page_node->EstimateResidentSetSize())
        .Record(ukm::UkmRecorder::Get());
  }
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

  DCHECK(base::Contains(page_node_info_map_, page_node));
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

  DCHECK(base::Contains(page_node_info_map_, page_node));
  std::unique_ptr<PageNodeInfo>& info = page_node_info_map_[page_node];
  info->current_lifecycle = page_node->GetLifecycleState();
  info->time_of_most_recent_state_change = base::TimeTicks::Now();
}

void PageTimelineMonitor::OnTypeChanged(const PageNode* page_node,
                                        PageType previous_state) {
  // When PageNodes are added, they have type kUnknown, and so it is when new
  // nodes get changed to being of type kTab that we can start using them.
  DCHECK(!(base::Contains(page_node_info_map_, page_node)));

  switch (page_node->GetType()) {
    case performance_manager::PageType::kTab:
      page_node_info_map_[page_node] =
          std::make_unique<PageNodeInfo>(base::TimeTicks::Now(), page_node);
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

void PageTimelineMonitor::OnTitleUpdated(const PageNode* page_node) {
  if (page_node->GetType() != performance_manager::PageType::kTab)
    return;

  DCHECK(base::Contains(page_node_info_map_, page_node));
  if (page_node_info_map_[page_node]->GetPageState() ==
      PageState::kBackground) {
    page_node_info_map_[page_node]->updated_title_or_favicon_in_background =
        true;
  }
}
void PageTimelineMonitor::OnFaviconUpdated(const PageNode* page_node) {
  if (page_node->GetType() != performance_manager::PageType::kTab)
    return;

  DCHECK(base::Contains(page_node_info_map_, page_node));
  if (page_node_info_map_[page_node]->GetPageState() ==
      PageState::kBackground) {
    page_node_info_map_[page_node]->updated_title_or_favicon_in_background =
        true;
  }
}

void PageTimelineMonitor::SetBatterySaverEnabled(bool enabled) {
  battery_saver_enabled_ = enabled;
}

}  // namespace performance_manager::metrics

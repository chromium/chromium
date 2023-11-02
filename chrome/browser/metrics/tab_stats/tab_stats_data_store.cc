// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_data_store.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace metrics {

namespace {

// Computes a new, unique, TabID.
TabStatsDataStore::TabID GetNewTabId() {
  static TabStatsDataStore::TabID web_contents_id = 0U;
  return ++web_contents_id;
}

}  // namespace

TabStatsDataStore::TabsStats::TabsStats()
    : total_tab_count(0U),
      total_tab_count_max(0U),
      max_tab_per_window(0U),
      window_count(0U),
      window_count_max(0U) {
  tab_discard_counts.fill(0U);
  tab_reload_counts.fill(0U);
}
TabStatsDataStore::TabsStats::TabsStats(const TabsStats& other) = default;
TabStatsDataStore::TabsStats& TabStatsDataStore::TabsStats::operator=(
    const TabsStats& other) = default;

TabStatsDataStore::TabStatsDataStore(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
  tab_stats_.total_tab_count_max =
      pref_service->GetInteger(::prefs::kTabStatsTotalTabCountMax);
  tab_stats_.max_tab_per_window =
      pref_service->GetInteger(::prefs::kTabStatsMaxTabsPerWindow);
  tab_stats_.window_count_max =
      pref_service->GetInteger(::prefs::kTabStatsWindowCountMax);

  // Loads discard/reload counters.
  tab_stats_.tab_discard_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::EXTERNAL)] =
      pref_service->GetInteger(::prefs::kTabStatsDiscardsExternal);
  tab_stats_.tab_discard_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::URGENT)] =
      pref_service->GetInteger(::prefs::kTabStatsDiscardsUrgent);
  tab_stats_.tab_reload_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::EXTERNAL)] =
      pref_service->GetInteger(::prefs::kTabStatsReloadsExternal);
  tab_stats_.tab_reload_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::URGENT)] =
      pref_service->GetInteger(::prefs::kTabStatsReloadsUrgent);
}

TabStatsDataStore::~TabStatsDataStore() {}

void TabStatsDataStore::OnWindowAdded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tab_stats_.window_count++;
  UpdateWindowCountMaxIfNeeded();
}

void TabStatsDataStore::OnWindowRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(tab_stats_.window_count, 0U);
  tab_stats_.window_count--;
}

void TabStatsDataStore::OnTabAdded(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  DCHECK(!base::Contains(existing_tabs_, web_contents));
  ++tab_stats_.total_tab_count;
  RecordSamplingMetaData();
  TabID tab_id = GetNewTabId();
  existing_tabs_.insert(std::make_pair(web_contents, tab_id));
  for (auto& interval_map : interval_maps_) {
    AddTabToIntervalMap(web_contents, tab_id,
                        /* existed_before_interval */ false,
                        interval_map.get());
  }
  UpdateTotalTabCountMaxIfNeeded();
}

void TabStatsDataStore::OnTabRemoved(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  DCHECK(base::Contains(existing_tabs_, web_contents));
  DCHECK_GT(tab_stats_.total_tab_count, 0U);
  --tab_stats_.total_tab_count;
  RecordSamplingMetaData();
  TabID web_contents_id = GetTabID(web_contents);
  existing_tabs_.erase(web_contents);
  for (auto& interval_map : interval_maps_) {
    auto iter = interval_map->find(web_contents_id);
    DCHECK(iter != interval_map->end());
    iter->second.exists_currently = false;
  }
}

void TabStatsDataStore::OnTabReplaced(content::WebContents* old_contents,
                                      content::WebContents* new_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(old_contents);
  DCHECK(new_contents);
  DCHECK(base::Contains(existing_tabs_, old_contents));
  DCHECK_GT(tab_stats_.total_tab_count, 0U);
  TabID old_contents_id = existing_tabs_[old_contents];
  existing_tabs_.erase(old_contents);
  existing_tabs_[new_contents] = old_contents_id;
}

void TabStatsDataStore::OnTabInteraction(content::WebContents* web_contents) {
  DCHECK(base::Contains(existing_tabs_, web_contents));
  TabID web_contents_id = GetTabID(web_contents);
  // Mark the tab as interacted with in all the intervals.
  for (auto& interval_map : interval_maps_) {
    DCHECK(base::Contains(*interval_map, web_contents_id));
    (*interval_map)[web_contents_id].interacted_during_interval = true;
  }
}

void TabStatsDataStore::OnTabIsAudibleChanged(
    content::WebContents* web_contents) {
  if (web_contents->IsCurrentlyAudible())
    OnTabAudibleOrVisible(web_contents);
}

void TabStatsDataStore::RecordSamplingMetaData() {
  tab_number_sample_meta_data_.Set(tab_stats_.total_tab_count);
}

void TabStatsDataStore::OnTabVisibilityChanged(
    content::WebContents* web_contents) {
  if (web_contents->GetVisibility() == content::Visibility::VISIBLE)
    OnTabAudibleOrVisible(web_contents);
}

void TabStatsDataStore::UpdateMaxTabsPerWindowIfNeeded(size_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (value <= tab_stats_.max_tab_per_window)
    return;
  tab_stats_.max_tab_per_window = value;
  pref_service_->SetInteger(::prefs::kTabStatsMaxTabsPerWindow, value);
}

void TabStatsDataStore::ResetMaximumsToCurrentState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set the maximums to 0 and call the Update* functions to reset it to the
  // current value and update the pref registry.
  tab_stats_.max_tab_per_window = 0;
  tab_stats_.window_count_max = 0;
  tab_stats_.total_tab_count_max = 0;
  UpdateTotalTabCountMaxIfNeeded();
  UpdateWindowCountMaxIfNeeded();

  // Iterates over the list of browsers to find the one with the maximum number
  // of tabs opened.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    UpdateMaxTabsPerWindowIfNeeded(
        static_cast<size_t>(browser->tab_strip_model()->count()));
  }
}

TabStatsDataStore::TabsStateDuringIntervalMap*
TabStatsDataStore::AddInterval() {
  // Creates the interval and initialize its data.
  std::unique_ptr<TabsStateDuringIntervalMap> interval_map =
      std::make_unique<TabsStateDuringIntervalMap>();
  ResetIntervalData(interval_map.get());
  interval_maps_.emplace_back(std::move(interval_map));
  return interval_maps_.back().get();
}

void TabStatsDataStore::ResetIntervalData(
    TabsStateDuringIntervalMap* interval_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interval_map);
  interval_map->clear();
  for (auto& iter : existing_tabs_)
    AddTabToIntervalMap(iter.first, GetTabID(iter.first), true, interval_map);
}

void TabStatsDataStore::OnTabDiscardStateChange(
    LifecycleUnitDiscardReason discard_reason,
    bool is_discarded) {
  size_t discard_reason_index = static_cast<size_t>(discard_reason);
  size_t& count = is_discarded
                      ? tab_stats_.tab_discard_counts[discard_reason_index]
                      : tab_stats_.tab_reload_counts[discard_reason_index];
  count++;
  switch (discard_reason) {
    case LifecycleUnitDiscardReason::EXTERNAL:
      if (is_discarded)
        pref_service_->SetInteger(::prefs::kTabStatsDiscardsExternal, count);
      else
        pref_service_->SetInteger(::prefs::kTabStatsReloadsExternal, count);
      break;
    case LifecycleUnitDiscardReason::URGENT:
      if (is_discarded)
        pref_service_->SetInteger(::prefs::kTabStatsDiscardsUrgent, count);
      else
        pref_service_->SetInteger(::prefs::kTabStatsReloadsUrgent, count);
      break;
  }
}

void TabStatsDataStore::ClearTabDiscardAndReloadCounts() {
  tab_stats_.tab_discard_counts.fill(0U);
  tab_stats_.tab_reload_counts.fill(0U);
  pref_service_->SetInteger(::prefs::kTabStatsDiscardsExternal, 0);
  pref_service_->SetInteger(::prefs::kTabStatsDiscardsUrgent, 0);
  pref_service_->SetInteger(::prefs::kTabStatsReloadsExternal, 0);
  pref_service_->SetInteger(::prefs::kTabStatsReloadsUrgent, 0);
}

absl::optional<TabStatsDataStore::TabID> TabStatsDataStore::GetTabIDForTesting(
    content::WebContents* web_contents) {
  if (!base::Contains(existing_tabs_, web_contents))
    return absl::nullopt;
  return GetTabID(web_contents);
}

void TabStatsDataStore::UpdateTotalTabCountMaxIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tab_stats_.total_tab_count <= tab_stats_.total_tab_count_max)
    return;
  tab_stats_.total_tab_count_max = tab_stats_.total_tab_count;
  pref_service_->SetInteger(::prefs::kTabStatsTotalTabCountMax,
                            tab_stats_.total_tab_count_max);
}

void TabStatsDataStore::UpdateWindowCountMaxIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tab_stats_.window_count <= tab_stats_.window_count_max)
    return;
  tab_stats_.window_count_max = tab_stats_.window_count;
  pref_service_->SetInteger(::prefs::kTabStatsWindowCountMax,
                            tab_stats_.window_count_max);
}

void TabStatsDataStore::AddTabToIntervalMap(
    content::WebContents* web_contents,
    TabID tab_id,
    bool existed_before_interval,
    TabsStateDuringIntervalMap* interval_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interval_map);
  DCHECK(web_contents);
  bool visible_or_audible =
      web_contents->GetVisibility() == content::Visibility::VISIBLE ||
      web_contents->IsCurrentlyAudible();

  auto& tab_state = (*interval_map)[tab_id];
  tab_state.existed_before_interval = existed_before_interval;
  tab_state.exists_currently = true;
  tab_state.visible_or_audible_during_interval = visible_or_audible;
  tab_state.interacted_during_interval = false;
}

TabStatsDataStore::TabID TabStatsDataStore::GetTabID(
    content::WebContents* web_contents) {
  DCHECK(base::Contains(existing_tabs_, web_contents));
  return existing_tabs_[web_contents];
}

void TabStatsDataStore::OnTabAudibleOrVisible(
    content::WebContents* web_contents) {
  DCHECK(base::Contains(existing_tabs_, web_contents));
  TabID web_contents_id = GetTabID(web_contents);
  // Mark the tab as visible or audible in all the intervals.
  for (auto& interval_map : interval_maps_) {
    DCHECK(base::Contains(*interval_map, web_contents_id));
    (*interval_map)[web_contents_id].visible_or_audible_during_interval = true;
  }
}

}  // namespace metrics

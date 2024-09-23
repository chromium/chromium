// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_data_store.h"

#include <utility>

#include "base/containers/contains.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace metrics {

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
  tab_stats_.tab_discard_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::PROACTIVE)] =
      pref_service->GetInteger(::prefs::kTabStatsDiscardsProactive);
  tab_stats_.tab_discard_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::SUGGESTED)] =
      pref_service->GetInteger(::prefs::kTabStatsDiscardsSuggested);
  tab_stats_.tab_reload_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::EXTERNAL)] =
      pref_service->GetInteger(::prefs::kTabStatsReloadsExternal);
  tab_stats_.tab_reload_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::URGENT)] =
      pref_service->GetInteger(::prefs::kTabStatsReloadsUrgent);
  tab_stats_.tab_reload_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::PROACTIVE)] =
      pref_service->GetInteger(::prefs::kTabStatsReloadsProactive);
  tab_stats_.tab_reload_counts[static_cast<size_t>(
      LifecycleUnitDiscardReason::SUGGESTED)] =
      pref_service->GetInteger(::prefs::kTabStatsReloadsSuggested);
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
  ++tab_stats_.total_tab_count;
  RecordSamplingMetaData();
  UpdateTotalTabCountMaxIfNeeded();
}

void TabStatsDataStore::OnTabRemoved(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  DCHECK_GT(tab_stats_.total_tab_count, 0U);
  --tab_stats_.total_tab_count;
  RecordSamplingMetaData();
}

void TabStatsDataStore::RecordSamplingMetaData() {
  tab_number_sample_meta_data_.Set(tab_stats_.total_tab_count);
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
    case LifecycleUnitDiscardReason::PROACTIVE:
      if (is_discarded) {
        pref_service_->SetInteger(::prefs::kTabStatsDiscardsProactive, count);
      } else {
        pref_service_->SetInteger(::prefs::kTabStatsReloadsProactive, count);
      }
      break;
    case LifecycleUnitDiscardReason::SUGGESTED:
      if (is_discarded) {
        pref_service_->SetInteger(::prefs::kTabStatsDiscardsSuggested, count);
      } else {
        pref_service_->SetInteger(::prefs::kTabStatsReloadsSuggested, count);
      }
      break;
  }
}

void TabStatsDataStore::ClearTabDiscardAndReloadCounts() {
  tab_stats_.tab_discard_counts.fill(0U);
  tab_stats_.tab_reload_counts.fill(0U);
  pref_service_->SetInteger(::prefs::kTabStatsDiscardsExternal, 0);
  pref_service_->SetInteger(::prefs::kTabStatsDiscardsUrgent, 0);
  pref_service_->SetInteger(::prefs::kTabStatsDiscardsProactive, 0);
  pref_service_->SetInteger(::prefs::kTabStatsDiscardsSuggested, 0);
  pref_service_->SetInteger(::prefs::kTabStatsReloadsExternal, 0);
  pref_service_->SetInteger(::prefs::kTabStatsReloadsUrgent, 0);
  pref_service_->SetInteger(::prefs::kTabStatsReloadsProactive, 0);
  pref_service_->SetInteger(::prefs::kTabStatsReloadsSuggested, 0);
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

}  // namespace metrics

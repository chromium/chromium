// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"

#include "base/metrics/metrics_hashes.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace tab_ranker {

using ukm::builders::TabManager_TabMetrics;

TabFeatures::TabFeatures() = default;

TabFeatures::~TabFeatures() = default;

TabFeatures::TabFeatures(const TabFeatures& other) = default;

void PopulateTabFeaturesToRankerExample(const TabFeatures& tab,
                                        assist_ranker::RankerExample* example) {
  auto& features = *example->mutable_features();

  features[TabManager_TabMetrics::kHasBeforeUnloadHandlerName].set_bool_value(
      tab.has_before_unload_handler);
  features[TabManager_TabMetrics::kHasFormEntryName].set_bool_value(
      tab.has_form_entry);
  features[TabManager_TabMetrics::kIsPinnedName].set_bool_value(tab.is_pinned);
  features[TabManager_TabMetrics::kKeyEventCountName].set_int32_value(
      tab.key_event_count);
  features[TabManager_TabMetrics::kMouseEventCountName].set_int32_value(
      tab.mouse_event_count);
  features[TabManager_TabMetrics::kNavigationEntryCountName].set_int32_value(
      tab.navigation_entry_count);
  features[TabManager_TabMetrics::kNumReactivationBeforeName].set_int32_value(
      tab.num_reactivations);
  // Nullable types indicate optional values; if not present, the corresponding
  // feature should not be set.
  if (tab.page_transition_core_type.has_value()) {
    features[TabManager_TabMetrics::kPageTransitionCoreTypeName]
        .set_int32_value(tab.page_transition_core_type.value());
  }
  features[TabManager_TabMetrics::kPageTransitionFromAddressBarName]
      .set_bool_value(tab.page_transition_from_address_bar);
  features[TabManager_TabMetrics::kPageTransitionIsRedirectName].set_bool_value(
      tab.page_transition_is_redirect);
  if (tab.site_engagement_score.has_value()) {
    features[TabManager_TabMetrics::kSiteEngagementScoreName].set_int32_value(
        tab.site_engagement_score.value());
  }
  features[TabManager_TabMetrics::kTimeFromBackgroundedName].set_int32_value(
      tab.time_from_backgrounded);
  features[TabManager_TabMetrics::kTouchEventCountName].set_int32_value(
      tab.touch_event_count);
  features[TabManager_TabMetrics::kWasRecentlyAudibleName].set_bool_value(
      tab.was_recently_audible);

  // Mru features.
  if (tab.total_tab_count > 0) {
    features[TabManager_TabMetrics::kMRUIndexName].set_int32_value(
        tab.mru_index);
    features[TabManager_TabMetrics::kTotalTabCountName].set_int32_value(
        tab.total_tab_count);

    features["NormalizedMRUIndex"].set_float_value(float(tab.mru_index) /
                                                   tab.total_tab_count);
  }

  // Window features.
  features[TabManager_TabMetrics::kWindowIsActiveName].set_bool_value(
      tab.window_is_active);
  features[TabManager_TabMetrics::kWindowShowStateName].set_int32_value(
      tab.window_show_state);
  features[TabManager_TabMetrics::kWindowTabCountName].set_int32_value(
      tab.window_tab_count);
  features[TabManager_TabMetrics::kWindowTypeName].set_int32_value(
      tab.window_type);
  // The new metric names are set as WindowTabCount, WindowType, in the ukm.xml,
  // but we still need old feature names TabCount, Type for current model.
  // TODO(charleszhao): remove old feature names once new model is trained.
  features["IsActive"].set_bool_value(tab.window_is_active);
  features["ShowState"].set_int32_value(tab.window_show_state);
  features["TabCount"].set_int32_value(tab.window_tab_count);
  features["Type"].set_int32_value(tab.window_type);

  features["TopDomain"].set_string_value(
      std::to_string(base::HashMetricName(tab.host)));
}

void PopulateTabFeaturesToUkmEntry(
    const TabFeatures& tab,
    ukm::builders::TabManager_TabMetrics* entry) {
  entry->SetHasBeforeUnloadHandler(tab.has_before_unload_handler);
  entry->SetHasFormEntry(tab.has_form_entry);
  entry->SetIsPinned(tab.is_pinned);
  entry->SetKeyEventCount(tab.key_event_count);
  entry->SetMouseEventCount(tab.mouse_event_count);
  entry->SetNavigationEntryCount(tab.navigation_entry_count);
  entry->SetNumReactivationBefore(tab.num_reactivations);
  if (tab.page_transition_core_type.has_value())
    entry->SetPageTransitionCoreType(tab.page_transition_core_type.value());
  entry->SetPageTransitionFromAddressBar(tab.page_transition_from_address_bar);
  entry->SetPageTransitionIsRedirect(tab.page_transition_is_redirect);
  if (tab.site_engagement_score.has_value())
    entry->SetSiteEngagementScore(tab.site_engagement_score.value());
  entry->SetTimeFromBackgrounded(tab.time_from_backgrounded);
  entry->SetTouchEventCount(tab.touch_event_count);
  entry->SetWasRecentlyAudible(tab.was_recently_audible);
  entry->SetWindowIsActive(tab.window_is_active);
  entry->SetWindowShowState(tab.window_show_state);
  entry->SetWindowTabCount(tab.window_tab_count);
  entry->SetWindowType(tab.window_type);

  if (tab.total_tab_count > 0) {
    entry->SetMRUIndex(tab.mru_index);
    entry->SetTotalTabCount(tab.total_tab_count);
  }
}

}  // namespace tab_ranker

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/fetch_and_rank_helper.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/auxiliary_search/auxiliary_search_provider.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "url/android/gurl_android.h"
#include "url/url_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/auxiliary_search/jni_headers/FetchAndRankHelper_jni.h"

using visited_url_ranking::Config;
using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::ResultStatus;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::URLVisitAggregatesTransformType;
using visited_url_ranking::URLVisitsMetadata;
using visited_url_ranking::URLVisitVariantHelper;
using visited_url_ranking::VisitedURLRankingService;
using visited_url_ranking::VisitedURLRankingServiceFactory;

namespace {
// Must match Java Tab.INVALID_TAB_ID.
static constexpr int kInvalidTabId = -1;

// 1 day in hours.
const int kHistoryAgeThresholdHoursDefaultValue = 24;
// 7 days in hours.
const int kTabAgeThresholdHoursDefaultValue = 168;

// Get the default age limit for the `url_type`.
base::TimeDelta GetDefaultAgeLimit(URLVisitAggregate::URLType url_type) {
  switch (url_type) {
    case URLVisitAggregate::URLType::kActiveLocalTab:
      return base::Hours(kTabAgeThresholdHoursDefaultValue);
    case URLVisitAggregate::URLType::kCCTVisit:
      return base::Hours(kHistoryAgeThresholdHoursDefaultValue);
    default:
      return base::TimeDelta();
  }
}

// Get the default visit duration limit for the `url_type`.
std::optional<base::TimeDelta> GetDefaultVisitDurationLimit(
    URLVisitAggregate::URLType url_type,
    bool skip_visit_duration_limit = false) {
  switch (url_type) {
    case URLVisitAggregate::URLType::kCCTVisit:
      // Needs to skip visit_duration check in
      // HistoryURLVisitDataFetcher#OnGotAnnotatedVisits when fetching an open
      // CCT whose visit_duration is 0. The visit duration will be set to the
      // history database once the CCT is closed.
      if (skip_visit_duration_limit) {
        return std::nullopt;
      }

      return base::Seconds(
          chrome::android::kAppIntegrationCCTVisitDurationLimitSecParam.Get());
    default:
      return std::nullopt;
  }
}

// Returns the maximum count of entries to donate.
int GetMaxDonationCount() {
  return chrome::android::kAppIntegrationMaxDonationCountParam.Get();
}

FetchOptions CreateFetchOptionsForTabDonation(
    const URLVisitAggregate::URLTypeSet& result_sources,
    const std::optional<GURL>& custom_tab_url,
    std::optional<base::Time> begin_time) {
  std::vector<URLVisitAggregatesTransformType> transforms{
      URLVisitAggregatesTransformType::kRecencyFilter,
      URLVisitAggregatesTransformType::kDefaultAppUrlFilter,
      URLVisitAggregatesTransformType::kHistoryBrowserTypeFilter,
  };

  if (base::FeatureList::IsEnabled(
          visited_url_ranking::features::
              kVisitedURLRankingHistoryVisibilityScoreFilter)) {
    transforms.push_back(
        URLVisitAggregatesTransformType::kHistoryVisibilityScoreFilter);
  }

  std::map<Fetcher, visited_url_ranking::FetchOptions::FetchSources>
      fetcher_sources;
  // Always useful for signals.
  fetcher_sources.emplace(Fetcher::kHistory,
                          visited_url_ranking::FetchOptions::kOriginSources);

  // If a URL of Custom Tab is provided, the fetcher only fetches history, not
  // open Tabs.
  if (custom_tab_url == std::nullopt) {
    fetcher_sources.emplace(
        Fetcher::kTabModel,
        visited_url_ranking::FetchOptions::FetchSources(
            {visited_url_ranking::URLVisit::Source::kLocal}));
  }

  // Sets the query duration to match the age limit for the local Tabs. It
  // allows getting the sensitivity scores of all qualified local Tabs.
  int query_duration = base::GetFieldTrialParamByFeatureAsInt(
      visited_url_ranking::features::kVisitedURLRankingService,
      visited_url_ranking::features::
          kVisitedURLRankingFetchDurationInHoursParam,
      kTabAgeThresholdHoursDefaultValue);
  std::map<URLVisitAggregate::URLType,
           visited_url_ranking::FetchOptions::ResultOption>
      result_map;
  for (URLVisitAggregate::URLType type : result_sources) {
    result_map[type] = visited_url_ranking::FetchOptions::ResultOption{
        .age_limit = GetDefaultAgeLimit(type),
        .visit_duration_limit =
            GetDefaultVisitDurationLimit(type, begin_time != std::nullopt)};
  }

  // Adjusts the begin time.
  base::Time new_begin_time =
      std::max(base::Time::Now() - base::Hours(query_duration),
               begin_time.value_or(base::Time()));

  return FetchOptions(std::move(result_map), std::move(fetcher_sources),
                      new_begin_time, std::move(transforms),
                      GetMaxDonationCount());
}

FetchOptions CreateFetchOptions(const std::optional<GURL>& custom_tab_url,
                                std::optional<base::Time> begin_time) {
  URLVisitAggregate::URLTypeSet expected_types;
  if (custom_tab_url == std::nullopt) {
    expected_types = {URLVisitAggregate::URLType::kActiveLocalTab,
                      URLVisitAggregate::URLType::kCCTVisit};
  } else {
    expected_types = {URLVisitAggregate::URLType::kCCTVisit};
  }
  return CreateFetchOptionsForTabDonation(expected_types, custom_tab_url,
                                          begin_time);
}

}  // namespace

FetchAndRankHelper::FetchAndRankHelper(
    VisitedURLRankingService* ranking_service,
    FetchResultCallback entries_callback,
    const std::optional<GURL>& custom_tab_url,
    std::optional<base::Time> begin_time)
    : ranking_service_(ranking_service),
      entries_callback_(std::move(entries_callback)),
      custom_tab_url_(custom_tab_url),
      fetch_options_(CreateFetchOptions(custom_tab_url, begin_time)),
      config_({.key = visited_url_ranking::kTabResumptionRankerKey}) {}

void FetchAndRankHelper::StartFetching() {
  ranking_service_->FetchURLVisitAggregates(
      fetch_options_,
      base::BindOnce(&FetchAndRankHelper::OnFetched, base::RetainedRef(this)));
}

FetchAndRankHelper::~FetchAndRankHelper() = default;

void FetchAndRankHelper::OnFetched(ResultStatus status,
                                   URLVisitsMetadata url_visits_metadata,
                                   std::vector<URLVisitAggregate> aggregates) {
  if (status != ResultStatus::kSuccess) {
    std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries;
    std::move(entries_callback_).Run(std::move(entries));
    return;
  }

  ranking_service_->RankURLVisitAggregates(
      config_, std::move(aggregates),
      base::BindOnce(&FetchAndRankHelper::OnRanked, base::RetainedRef(this),
                     std::move(url_visits_metadata)));
}

void FetchAndRankHelper::OnRanked(URLVisitsMetadata url_visits_metadata,
                                  ResultStatus status,
                                  std::vector<URLVisitAggregate> aggregates) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries;
  if (status != ResultStatus::kSuccess) {
    std::move(entries_callback_).Run(std::move(entries));
    return;
  }

  for (const URLVisitAggregate& aggregate : aggregates) {
    if (aggregate.fetcher_data_map.empty()) {
      continue;
    }
    // TODO(crbug.com/337858147): Choose representative member. For now, just
    // take the first one.
    const auto& fetcher_entry = *aggregate.fetcher_data_map.begin();
    std::visit(
        URLVisitVariantHelper{
            [&](const URLVisitAggregate::TabData& tab_data) {
              bool is_local_tab =
                  (tab_data.last_active_tab.id != kInvalidTabId);
              if (!is_local_tab) {
                return;
              }

              entries.push_back(Java_FetchAndRankHelper_addDataEntry(
                  env,

                  static_cast<int>(AuxiliarySearchEntryType::kTab),
                  url::GURLAndroid::FromNativeGURL(
                      env, tab_data.last_active_tab.visit.url),
                  base::android::ConvertUTF16ToJavaString(
                      env, tab_data.last_active_tab.visit.title),
                  tab_data.last_active.InMillisecondsSinceUnixEpoch(),
                  tab_data.last_active_tab.id, /* appId= */ nullptr,
                  kInvalidTabId));
            },
            [&](const URLVisitAggregate::HistoryData& history_data) {
              bool is_custom_tab =
                  history_data.last_visited.context_annotations.on_visit
                          .browser_type == history::VisitContextAnnotations::
                                               BrowserType::kCustomTab ||
                  history_data.last_app_id != std::nullopt ||
                  (custom_tab_url_ != std::nullopt &&
                   custom_tab_url_.value() ==
                       history_data.last_visited.url_row.url());
              if (!is_custom_tab) {
                return;
              }

              entries.push_back(Java_FetchAndRankHelper_addDataEntry(
                  env,

                  static_cast<int>(AuxiliarySearchEntryType::kCustomTab),
                  url::GURLAndroid::FromNativeGURL(
                      env, history_data.last_visited.url_row.url()),
                  base::android::ConvertUTF16ToJavaString(
                      env, history_data.last_visited.url_row.title()),
                  history_data.last_visited.visit_row.visit_time
                      .InMillisecondsSinceUnixEpoch(),
                  kInvalidTabId,
                  history_data.last_app_id
                      ? base::android::ConvertUTF8ToJavaString(
                            env, *history_data.last_app_id)
                      : base::android::ConvertUTF8ToJavaString(env,
                                                               std::string()),
                  std::abs(static_cast<int>(base::Hash(aggregate.url_key)))));
            }},
        fetcher_entry.second);
  }

  std::move(entries_callback_).Run(std::move(entries));
}

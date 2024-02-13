// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_search_provider.h"

#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/app_search_data_source.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

namespace {

// Checks if current locale is non Latin locales.
bool IsNonLatinLocale(std::string_view locale) {
  // A set of of non Latin locales. This set is used to select appropriate
  // algorithm for app search.
  static constexpr char kNonLatinLocales[][6] = {
      "am", "ar", "be", "bg", "bn",    "el",    "fa",   "gu", "hi",
      "hy", "iw", "ja", "ka", "kk",    "km",    "kn",   "ko", "ky",
      "lo", "mk", "ml", "mn", "mr",    "my",    "pa",   "ru", "sr",
      "ta", "te", "th", "uk", "zh-CN", "zh-HK", "zh-TW"};
  return base::Contains(kNonLatinLocales, locale);
}

}  // namespace

AppSearchProvider::AppSearchProvider(AppSearchDataSource* data_source)
    : SearchProvider(SearchCategory::kApps), data_source_(data_source) {
  app_updates_subscription_ =
      data_source_->SubscribeToAppUpdates(base::BindRepeating(
          &AppSearchProvider::UpdateResults, base::Unretained(this)));
}

AppSearchProvider::~AppSearchProvider() = default;

void AppSearchProvider::Start(const std::u16string& query) {
  query_ = query;
  query_start_time_ = base::TimeTicks::Now();
  // We only need to record app search latency for queries started by user.
  record_query_uma_ = true;

  {
    // Prevent `UpdateResults()` from running as a result of a data source
    // refresh callback to avoid double update.
    base::AutoReset<bool> auto_reset(&updates_blocked_, true);
    data_source_->RefreshIfNeeded();
  }

  UpdateResults();
}

void AppSearchProvider::StopQuery() {
  query_.clear();
  record_query_uma_ = false;
}

ash::AppListSearchResultType AppSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kInstalledApp;
}

void AppSearchProvider::UpdateResults() {
  if (updates_blocked_ || query_.empty())
    return;

  SearchProvider::Results new_results;

  const bool use_exact_match =
      app_list_features::IsExactMatchForNonLatinLocaleEnabled() &&
      IsNonLatinLocale(base::i18n::GetConfiguredLocale());

  if (use_exact_match) {
    new_results = data_source_->GetExactMatches(query_);
  } else {
    new_results = data_source_->GetFuzzyMatches(query_);
  }

  if (record_query_uma_) {
    record_query_uma_ = false;
    UMA_HISTOGRAM_TIMES("Apps.AppList.AppSearchProvider.QueryTime",
                        base::TimeTicks::Now() - query_start_time_);
  }

  SwapResults(&new_results);
}

}  // namespace app_list

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_metrics.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/metrics/histogram_macros.h"

namespace {

// The following constants affect logging, and  should not be changed without
// deprecating the related UMA histograms.

const char kAppListSearchResultOpenTypeHistogram[] =
    "Apps.AppListSearchResultOpenTypeV2";
const char kAppListSearchResultOpenTypeHistogramInTablet[] =
    "Apps.AppListSearchResultOpenTypeV2.TabletMode";
const char kAppListSearchResultOpenTypeHistogramInClamshell[] =
    "Apps.AppListSearchResultOpenTypeV2.ClamshellMode";
const char kAppListSuggestionChipOpenTypeHistogramInClamshell[] =
    "Apps.AppListSuggestedChipOpenType.ClamshellMode";
const char kAppListSuggestionChipOpenTypeHistogramInTablet[] =
    "Apps.AppListSuggestedChipOpenType.TabletMode";
const char kAppListZeroStateSuggestionOpenTypeHistogram[] =
    "Apps.AppList.ZeroStateSuggestionOpenType";
const char kAppListDefaultSearchResultOpenTypeHistogram[] =
    "Apps.AppListDefaultSearchResultOpenType";
// The UMA histogram that logs the length of user typed queries app list
// launcher issues to the search providers.
constexpr char kAppListLauncherIssuedSearchQueryLength[] =
    "Apps.AppListLauncherIssuedSearchQueryLength";
// The UMA histogram that logs the length of the query that resulted in an app
// launch from search box.
constexpr char kSearchQueryLengthAppLaunch[] =
    "Apps.AppList.SearchQueryLength.Apps";
// The UMA histogram that logs the number of app launches from the search box
// with non-empty queries.
constexpr char kSearchSuccessAppLaunch[] = "Apps.AppList.SearchSuccess.Apps";

// Maximum query length logged for user typed query in characters.
constexpr int kMaxLoggedUserQueryLength = 20;

}  // namespace

namespace ash {

void RecordSearchResultOpenTypeHistogram(
    ash::AppListLaunchedFrom launch_location,
    SearchResultType type,
    bool is_tablet_mode) {
  if (type == SEARCH_RESULT_TYPE_BOUNDARY) {
    NOTREACHED();
    return;
  }

  switch (launch_location) {
    case ash::AppListLaunchedFrom::kLaunchedFromSearchBox:
      UMA_HISTOGRAM_ENUMERATION(kAppListSearchResultOpenTypeHistogram, type,
                                SEARCH_RESULT_TYPE_BOUNDARY);
      if (is_tablet_mode) {
        UMA_HISTOGRAM_ENUMERATION(kAppListSearchResultOpenTypeHistogramInTablet,
                                  type, SEARCH_RESULT_TYPE_BOUNDARY);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            kAppListSearchResultOpenTypeHistogramInClamshell, type,
            SEARCH_RESULT_TYPE_BOUNDARY);
      }
      break;
    case ash::AppListLaunchedFrom::kLaunchedFromSuggestionChip:
      if (is_tablet_mode) {
        UMA_HISTOGRAM_ENUMERATION(
            kAppListSuggestionChipOpenTypeHistogramInTablet, type,
            SEARCH_RESULT_TYPE_BOUNDARY);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            kAppListSuggestionChipOpenTypeHistogramInClamshell, type,
            SEARCH_RESULT_TYPE_BOUNDARY);
      }
      break;
    case ash::AppListLaunchedFrom::kLaunchedFromShelf:
    case ash::AppListLaunchedFrom::kLaunchedFromGrid:
      // Search results don't live in the shelf or the app grid.
      NOTREACHED();
      break;
  }
}

void RecordDefaultSearchResultOpenTypeHistogram(SearchResultType type) {
  if (type == SEARCH_RESULT_TYPE_BOUNDARY) {
    NOTREACHED();
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kAppListDefaultSearchResultOpenTypeHistogram, type,
                            SEARCH_RESULT_TYPE_BOUNDARY);
}

void RecordZeroStateSuggestionOpenTypeHistogram(SearchResultType type) {
  UMA_HISTOGRAM_ENUMERATION(kAppListZeroStateSuggestionOpenTypeHistogram, type,
                            SEARCH_RESULT_TYPE_BOUNDARY);
}

void RecordLauncherIssuedSearchQueryLength(int query_length) {
  if (query_length > 0) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        kAppListLauncherIssuedSearchQueryLength,
        std::min(query_length, kMaxLoggedUserQueryLength),
        kMaxLoggedUserQueryLength);
  }
}

void RecordSuccessfulAppLaunchUsingSearch(
    ash::AppListLaunchedFrom launched_from,
    int query_length) {
  if (query_length > 0) {
    UMA_HISTOGRAM_ENUMERATION(kSearchSuccessAppLaunch, launched_from);
    UMA_HISTOGRAM_COUNTS_100(kSearchQueryLengthAppLaunch, query_length);
  }
}

}  // namespace ash

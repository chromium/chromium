// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_metrics.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace {

// The following constants affect logging, and should not be changed without
// deprecating the related UMA histograms.

const char kAppListSearchResultOpenTypeHistogram[] =
    "Apps.AppListSearchResultOpenTypeV2";
const char kAppListSearchResultOpenTypeHistogramInTablet[] =
    "Apps.AppListSearchResultOpenTypeV2.TabletMode";
const char kAppListSearchResultOpenTypeHistogramInClamshell[] =
    "Apps.AppListSearchResultOpenTypeV2.ClamshellMode";
const char kAppListContinueTaskOpenTypeHistogramInClamshell[] =
    "Apps.AppListContinueTaskOpenType.ClamshellMode";
const char kAppListContinueTaskOpenTypeHistogramInTablet[] =
    "Apps.AppListContinueTaskOpenType.TabletMode";
const char kAppListZeroStateSuggestionOpenTypeHistogram[] =
    "Apps.AppList.ZeroStateSuggestionOpenType";
const char kAppListDefaultSearchResultOpenTypeHistogram[] =
    "Apps.AppListDefaultSearchResultOpenType";
// The UMA histogram that logs the length of user typed queries app list
// launcher issues to the search providers.
constexpr char kAppListLauncherIssuedSearchQueryLength[] =
    "Apps.AppListLauncherIssuedSearchQueryLength";
// The UMA histogram that logs the length of the query that resulted in a click.
constexpr char kAppListLauncherClickedSearchQueryLength[] =
    "Apps.AppListLauncherClickedSearchQueryLength";
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

constexpr char kClamshellPrefOrderClearActionHistogram[] =
    "Apps.Launcher.AppListSortClearAction.ClamshellMode";
constexpr char kTabletPrefOrderClearActionHistogram[] =
    "Apps.Launcher.AppListSortClearAction.TabletMode";

constexpr char kClamshellAppListSortOrderOnSessionStartHistogram[] =
    "Apps.AppList.SortOrderOnSessionStart.ClamshellMode";
constexpr char kTabletAppListSortOrderOnSessionStartHistogram[] =
    "Apps.AppList.SortOrderOnSessionStart.TabletMode";

constexpr char kAppListSortDiscoveryDurationAfterNudge[] =
    "Apps.AppList.SortDiscoveryDurationAfterEducationNudge";

constexpr char kAppListSortDiscoveryDurationAfterActivation[] =
    "Apps.AppList."
    "AppListSortDiscoveryDurationAfterActivation";

constexpr char kAppListSortDiscoveryDurationAfterNudgeClamshell[] =
    "Apps.AppList.SortDiscoveryDurationAfterEducationNudgeV2.ClamshellMode";
constexpr char kAppListSortDiscoveryDurationAfterNudgeTablet[] =
    "Apps.AppList.SortDiscoveryDurationAfterEducationNudgeV2.TabletMode";

// LINT.IfChange(SearchSessionConclusion)
std::string SearchSessionConclusionToString(
    SearchSessionConclusion conclusion) {
  switch (conclusion) {
    case SearchSessionConclusion::kQuit:
      return "Quit";
    case SearchSessionConclusion::kLaunch:
      return "Launch";
    case SearchSessionConclusion::kAnswerCardSeen:
      return "AnswerCardSeen";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/apps/enums.xml:LauncherSearchSessionConclusion)

bool IsAppListShowSourceUserTriggered(AppListShowSource show_source) {
  switch (show_source) {
    case AppListShowSource::kScrollFromShelf:
    case AppListShowSource::kSearchKey:
    case AppListShowSource::kSearchKeyFullscreen_DEPRECATED:
    case AppListShowSource::kShelfButton:
    case AppListShowSource::kShelfButtonFullscreen_DEPRECATED:
    case AppListShowSource::kSwipeFromShelf:
      return true;
    case AppListShowSource::kTabletMode:
    case AppListShowSource::kAssistantEntryPoint:
    case AppListShowSource::kBrowser:
    case AppListShowSource::kWelcomeTour:
      return false;
  }
  NOTREACHED();
}

void RecordSearchResultOpenTypeHistogram(AppListLaunchedFrom launch_location,
                                         SearchResultType type,
                                         bool is_tablet_mode) {
  if (type == SEARCH_RESULT_TYPE_BOUNDARY) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  switch (launch_location) {
    case AppListLaunchedFrom::kLaunchedFromSearchBox:
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
    case AppListLaunchedFrom::kLaunchedFromContinueTask:
      if (is_tablet_mode) {
        UMA_HISTOGRAM_ENUMERATION(kAppListContinueTaskOpenTypeHistogramInTablet,
                                  type, SEARCH_RESULT_TYPE_BOUNDARY);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            kAppListContinueTaskOpenTypeHistogramInClamshell, type,
            SEARCH_RESULT_TYPE_BOUNDARY);
      }
      break;
    case AppListLaunchedFrom::kLaunchedFromShelf:
    case AppListLaunchedFrom::kLaunchedFromGrid:
    case AppListLaunchedFrom::kLaunchedFromRecentApps:
    case AppListLaunchedFrom::DEPRECATED_kLaunchedFromSuggestionChip:
    case AppListLaunchedFrom::kLaunchedFromQuickAppAccess:
    case AppListLaunchedFrom::kLaunchedFromAppsCollections:
    case AppListLaunchedFrom::kLaunchedFromDiscoveryChip:
      // Search results don't live in the shelf, the app grid, apps collections
      // or recent apps.
      NOTREACHED();
  }
}

void RecordDefaultSearchResultOpenTypeHistogram(SearchResultType type) {
  if (type == SEARCH_RESULT_TYPE_BOUNDARY) {
    DUMP_WILL_BE_NOTREACHED();
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

void RecordLauncherClickedSearchQueryLength(int query_length) {
  if (query_length > 0) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        kAppListLauncherClickedSearchQueryLength,
        std::min(query_length, kMaxLoggedUserQueryLength),
        kMaxLoggedUserQueryLength);
  }
}

void RecordSuccessfulAppLaunchUsingSearch(AppListLaunchedFrom launched_from,
                                          int query_length) {
  if (query_length > 0) {
    UMA_HISTOGRAM_ENUMERATION(kSearchSuccessAppLaunch, launched_from);
    UMA_HISTOGRAM_COUNTS_100(kSearchQueryLengthAppLaunch, query_length);
  }
}

void ReportPrefOrderClearAction(AppListOrderUpdateEvent action,
                                bool in_tablet) {
  if (in_tablet) {
    base::UmaHistogramEnumeration(kTabletPrefOrderClearActionHistogram, action);
  } else {
    base::UmaHistogramEnumeration(kClamshellPrefOrderClearActionHistogram,
                                  action);
  }
}

void RecordFirstSearchResult(SearchResultType type, bool in_tablet) {
  if (in_tablet) {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.AppList.LaunchedResultInNewUsersFirstSearch.TabletMode", type,
        SEARCH_RESULT_TYPE_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.AppList.LaunchedResultInNewUsersFirstSearch.ClamshellMode", type,
        SEARCH_RESULT_TYPE_BOUNDARY);
  }
}

void ReportPrefSortOrderOnSessionStart(AppListSortOrder permanent_order,
                                       bool in_tablet) {
  // NOTE: kNameReverseAlphabetical is not used outside of tests.
  DCHECK(permanent_order != AppListSortOrder::kNameReverseAlphabetical);

  if (in_tablet) {
    base::UmaHistogramEnumeration(
        kTabletAppListSortOrderOnSessionStartHistogram, permanent_order);
  } else {
    base::UmaHistogramEnumeration(
        kClamshellAppListSortOrderOnSessionStartHistogram, permanent_order);
  }
}

}  // namespace ash

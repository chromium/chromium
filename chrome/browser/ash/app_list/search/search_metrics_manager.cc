// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_metrics_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_list/search/common/string_util.h"
#include "chrome/browser/ash/app_list/search/ranking/constants.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/search_metrics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/drive_pref_names.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/prefs/pref_service.h"

namespace app_list {
namespace {

using Result = ash::AppListNotifier::Result;
using Location = ash::AppListNotifier::Location;
using Action = SearchMetricsManager::Action;

std::string GetViewString(Location location, const std::u16string& raw_query) {
  std::u16string query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
  switch (location) {
    case Location::kList:
      return query.empty() ? "ListZeroState" : "ListSearch";
    case Location::kAnswerCard:
      return "AnswerCard";
    case Location::kRecentApps:
      return "RecentApps";
    case Location::kContinue:
      return "Continue";
    case Location::kImage:
      return "Image";
    default:
      LogError(Error::kUntrackedLocation);
      return "Untracked";
  }
}

std::set<ash::SearchResultType> TypeSet(const std::vector<Result>& results) {
  std::set<ash::SearchResultType> types;
  for (const auto& result : results) {
    types.insert(result.type);
  }
  return types;
}

std::set<ash::ContinueFileSuggestionType> ContinueFileTypeSet(
    const std::vector<Result>& results) {
  std::set<ash::ContinueFileSuggestionType> types;
  for (const auto& result : results) {
    if (result.continue_file_type) {
      types.insert(*result.continue_file_type);
    }
  }
  return types;
}

// Log an action on a set of search result types.
void LogTypeActions(const std::string& action_name,
                    Location location,
                    const std::u16string& query,
                    const std::set<ash::SearchResultType>& types) {
  const std::string histogram_name = base::StrCat(
      {kHistogramPrefix, GetViewString(location, query), ".", action_name});

  for (auto type : types) {
    if (type == ash::SEARCH_RESULT_TYPE_BOUNDARY) {
      LogError(Error::kUntypedResult);
    } else {
      base::UmaHistogramEnumeration(histogram_name, type,
                                    ash::SEARCH_RESULT_TYPE_BOUNDARY);
    }
  }
}

void LogContinueFileTypeActions(
    const std::string& action_name,
    Location location,
    const std::set<ash::ContinueFileSuggestionType>& continue_file_types) {
  if (location != Location::kContinue) {
    return;
  }

  const std::string histogram_name = base::StrCat(
      {kHistogramPrefix, "Continue.FileSuggestionType.", action_name});
  for (auto type : continue_file_types) {
    base::UmaHistogramEnumeration(histogram_name, type);
  }
}

// Log an action for a search result view as a whole.
void LogViewAction(Location location,
                   const std::u16string& query,
                   Action action) {
  const std::string histogram_name =
      base::StrCat({kHistogramPrefix, GetViewString(location, query)});
  base::UmaHistogramEnumeration(histogram_name, action);
}

void LogIsDriveEnabled(Profile* profile) {
  // Logs false for disabled and true for enabled, corresponding to the
  // BooleanEnabled enum.
  base::UmaHistogramBoolean(
      "Apps.AppList.ContinueIsDriveEnabled",
      !profile->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive));
}

void LogContinueMetrics(const std::vector<Result>& results) {
  int drive_count = 0;
  int local_count = 0;
  int help_app_count = 0;
  for (const auto& result : results) {
    switch (result.type) {
      case ash::SearchResultType::ZERO_STATE_DRIVE:
        ++drive_count;
        break;
      case ash::SearchResultType::ZERO_STATE_FILE:
        ++local_count;
        break;
      case ash::SearchResultType::HELP_APP_UPDATES:
        ++help_app_count;
        break;
      case ash::SearchResultType::DESKS_ADMIN_TEMPLATE:
        break;
      default:
        NOTREACHED_IN_MIGRATION() << static_cast<int>(result.type);
    }
  }

  base::UmaHistogramExactLinear("Apps.AppList.Search.ContinueResultCount.Total",
                                results.size(), 10);
  base::UmaHistogramExactLinear("Apps.AppList.Search.ContinueResultCount.Drive",
                                drive_count, 10);
  base::UmaHistogramExactLinear("Apps.AppList.Search.ContinueResultCount.Local",
                                local_count, 10);
  base::UmaHistogramExactLinear(
      "Apps.AppList.Search.ContinueResultCount.HelpApp", help_app_count, 10);
  base::UmaHistogramBoolean("Apps.AppList.Search.DriveContinueResultsShown",
                            drive_count > 0);
}

}  // namespace

SearchMetricsManager::SearchMetricsManager(Profile* profile,
                                           ash::AppListNotifier* notifier) {
  if (notifier) {
    observation_.Observe(notifier);
  } else {
    LogError(Error::kMissingNotifier);
  }

  if (profile)
    LogIsDriveEnabled(profile);
}

SearchMetricsManager::~SearchMetricsManager() = default;

void SearchMetricsManager::OnImpression(Location location,
                                        const std::vector<Result>& results,
                                        const std::u16string& query) {
  LogTypeActions("Impression", location, query, TypeSet(results));
  LogContinueFileTypeActions("Impression", location,
                             ContinueFileTypeSet(results));
  if (!results.empty())
    LogViewAction(location, query, Action::kImpression);
  if (location == Location::kContinue)
    LogContinueMetrics(results);
}

void SearchMetricsManager::OnAbandon(Location location,
                                     const std::vector<Result>& results,
                                     const std::u16string& query) {
  LogTypeActions("Abandon", location, query, TypeSet(results));
  LogContinueFileTypeActions("Abandon", location, ContinueFileTypeSet(results));
  if (!results.empty())
    LogViewAction(location, query, Action::kAbandon);
}

void SearchMetricsManager::OnLaunch(Location location,
                                    const Result& launched,
                                    const std::vector<Result>& shown,
                                    const std::u16string& query) {
  LogViewAction(location, query, Action::kLaunch);

  // Record an ignore for all result types in this view. If other views are
  // shown, they are handled by OnIgnore.
  std::set<ash::SearchResultType> types;
  for (const auto& result : shown) {
    if (result.type != launched.type) {
      types.insert(result.type);
    }
  }

  std::set<ash::ContinueFileSuggestionType> continue_file_types;
  for (const auto& result : shown) {
    if (result.continue_file_type &&
        result.continue_file_type != launched.continue_file_type) {
      continue_file_types.insert(*result.continue_file_type);
    }
  }

  LogTypeActions("Ignore", location, query, types);
  LogContinueFileTypeActions("Ignore", location, continue_file_types);
  LogTypeActions("Launch", location, query, {launched.type});
  if (launched.continue_file_type) {
    LogContinueFileTypeActions("Launch", location,
                               {*launched.continue_file_type});
  }

  // Record the launch index.
  int launched_index = -1;
  for (size_t i = 0; i < shown.size(); ++i) {
    if (shown[i].id == launched.id) {
      launched_index = base::checked_cast<int>(i);
      break;
    }
  }
  const std::string histogram_name = base::StrCat(
      {kHistogramPrefix, GetViewString(location, query), ".LaunchIndex"});
  base::UmaHistogramExactLinear(histogram_name, launched_index, 50);
}

void SearchMetricsManager::OnIgnore(Location location,
                                    const std::vector<Result>& results,
                                    const std::u16string& query) {
  // We have no two concurrently displayed views showing the same result types,
  // so it's safe to log an ignore for all result types here.
  LogTypeActions("Ignore", location, query, TypeSet(results));
  if (!results.empty())
    LogViewAction(location, query, Action::kIgnore);
}

// Log the length of the last query that led to the clicked result - for zero
// state search results, log 0.
void SearchMetricsManager::OnOpen(ash::AppListSearchResultType result_type,
                                  const std::u16string& query) {
  if (ash::IsZeroStateResultType(result_type)) {
    ash::RecordLauncherClickedSearchQueryLength(0);
  } else {
    ash::RecordLauncherClickedSearchQueryLength(query.length());
  }
}

void SearchMetricsManager::OnTrain(LaunchData& launch_data,
                                   const std::string& query) {
  // Record a structured metrics event.
  const base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  metrics::structured::StructuredMetricsClient::Record(
      std::move(metrics::structured::events::v2::launcher_usage::LauncherUsage()
                    .SetTarget(NormalizeId(launch_data.id))
                    .SetApp(last_launched_app_id_)
                    .SetSearchQuery(query)
                    .SetSearchQueryLength(query.size())
                    .SetProviderType(static_cast<int>(launch_data.result_type))
                    .SetHour(now_exploded.hour)
                    .SetScore(launch_data.score)));

  // Only record the last launched app if the hashed logging feature flag is
  // enabled, because it is only used by hashed logging.
  if (IsAppListSearchResultAnApp(launch_data.result_type)) {
    last_launched_app_id_ = NormalizeId(launch_data.id);
  } else if (launch_data.result_type ==
             ash::AppListSearchResultType::kArcAppShortcut) {
    last_launched_app_id_ = RemoveAppShortcutLabel(NormalizeId(launch_data.id));
  }
}

void SearchMetricsManager::OnSearchResultsUpdated(const Scoring& scoring) {
  double score = scoring.BestMatchScore();
  if (search_features::IsLauncherKeywordExtractionScoringEnabled()) {
    UMA_HISTOGRAM_BOOLEAN(
        "Apps.AppList.Scoring.ScoreAboveBestMatchThresholdWithKeywordRanking",
        score > kBestMatchThresholdWithKeywordRanking);
  } else {
    UMA_HISTOGRAM_BOOLEAN(
        "Apps.AppList.Scoring."
        "ScoreAboveBestMatchThresholdWithoutKeywordRanking",
        score > kBestMatchThreshold);
  }
}

}  // namespace app_list

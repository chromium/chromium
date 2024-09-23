// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_session_metrics_manager.h"

#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/search_metrics_util.h"

namespace app_list {
namespace {

using Result = ash::AppListNotifier::Result;
using Location = ash::AppListNotifier::Location;

}  // namespace

SearchSessionMetricsManager::SearchSessionMetricsManager(
    Profile* profile,
    ash::AppListNotifier* notifier) {
  if (notifier) {
    observation_.Observe(notifier);
  } else {
    LogSessionError(Error::kMissingNotifier);
  }
}

SearchSessionMetricsManager::~SearchSessionMetricsManager() = default;

void SearchSessionMetricsManager::EndSearchSession(
    const std::u16string& query) {
  std::string show_source = GetAppListOpenMethod(
      ash::AppListController::Get()->LastAppListShowSource());

  base::UmaHistogramEnumeration(
      base::StrCat({kSessionHistogramPrefix, show_source}), session_result_);

  base::UmaHistogramExactLinear("Apps.AppList.Keyword.NumberOfKeywordsInQuery",
                                ExtractKeywords(query).size(), 100);

  // Log query length.
  base::UmaHistogramExactLinear("Apps.AppList.Search.Session2.QueryLength",
                                query.size(),
                                kMaxLoggedQueryLengthOnSessionConclusion);
  // Log query length, split by SearchSessionConclusion.
  base::UmaHistogramExactLinear(
      base::StrCat({"Apps.AppList.Search.Session2.QueryLength.",
                    ash::SearchSessionConclusionToString(session_result_)}),
      query.size(), kMaxLoggedQueryLengthOnSessionConclusion);

  session_result_ = ash::SearchSessionConclusion::kQuit;
  session_active_ = false;
}

void SearchSessionMetricsManager::OnSearchSessionStarted() {
  session_active_ = true;
}

void SearchSessionMetricsManager::OnSearchSessionEnded(
    const std::u16string& query) {
  if (!session_active_) {
    LOG(ERROR) << "A request for a launcher search session to end has been "
                  "made when there was no active session.";
    return;
  }
  EndSearchSession(query);
}

void SearchSessionMetricsManager::OnSeen(Location location,
                                         const std::vector<Result>& results,
                                         const std::u16string& query) {
  if (location == Location::kAnswerCard) {
    DCHECK(session_active_);
    session_result_ = ash::SearchSessionConclusion::kAnswerCardSeen;
  }
}

void SearchSessionMetricsManager::OnLaunch(Location location,
                                           const Result& launched,
                                           const std::vector<Result>& shown,
                                           const std::u16string& query) {
  if (location == Location::kList) {
    DCHECK(session_active_);
    session_result_ = ash::SearchSessionConclusion::kLaunch;
  }
}

}  // namespace app_list

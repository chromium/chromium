// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_session_metrics_manager.h"

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
    LogError(Error::kMissingNotifier);
  }
}

SearchSessionMetricsManager::~SearchSessionMetricsManager() = default;

void SearchSessionMetricsManager::EndSearchSession() {
  // TODO (crbug/1380563) Log search metrics
  session_active_ = false;
  session_answer_card_impression_ = false;
}

void SearchSessionMetricsManager::OnSearchSessionStarted() {
  session_active_ = true;
}

void SearchSessionMetricsManager::OnSearchSessionEnded() {
  EndSearchSession();
}

void SearchSessionMetricsManager::OnImpression(
    Location location,
    const std::vector<Result>& results,
    const std::u16string& query) {
  if (location == Location::kAnswerCard) {
    DCHECK(session_active_);
    session_answer_card_impression_ = true;
  }
}

void SearchSessionMetricsManager::OnLaunch(Location location,
                                           const Result& launched,
                                           const std::vector<Result>& shown,
                                           const std::u16string& query) {
  if (location == Location::kList) {
    DCHECK(session_active_);
    result_launched_ = true;
    // TODO (crbug/1380563) Log search metrics
  }
  EndSearchSession();
}

}  // namespace app_list

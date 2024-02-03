// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_SESSION_METRICS_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_SESSION_METRICS_MANAGER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/scoped_observation.h"

class Profile;

namespace app_list {

const int kMaxLoggedQueryLengthOnSessionConclusion = 20;

// Records launcher search backend metrics. This includes impression,
// abandonment, and launch information reported by the AppListNotifier.
class SearchSessionMetricsManager : ash::AppListNotifier::Observer {
 public:
  using Result = ash::AppListNotifier::Result;
  using Location = ash::AppListNotifier::Location;

  SearchSessionMetricsManager(Profile* profile, ash::AppListNotifier* notifier);
  ~SearchSessionMetricsManager() override;

  SearchSessionMetricsManager(const SearchSessionMetricsManager&) = delete;
  SearchSessionMetricsManager& operator=(const SearchSessionMetricsManager&) =
      delete;

  void EndSearchSession(const std::u16string& query);

  // ash::AppListNotifier::Observer:
  void OnSearchSessionStarted() override;
  void OnSearchSessionEnded(const std::u16string& query) override;
  void OnSeen(Location location,
              const std::vector<Result>& results,
              const std::u16string& query) override;
  void OnLaunch(Location location,
                const Result& launched,
                const std::vector<Result>& shown,
                const std::u16string& query) override;

 private:
  // Whether the metrics manager is tracking an active search session.
  bool session_active_ = false;
  // Tracks the metric recorded when EndSearchSession() is called.
  ash::SearchSessionConclusion session_result_ =
      ash::SearchSessionConclusion::kQuit;

  base::ScopedObservation<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observation_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_SESSION_METRICS_MANAGER_H_

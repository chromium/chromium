// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_METRICS_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_METRICS_MANAGER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/scoped_observation.h"

class Profile;

namespace app_list {

// Records launcher search backend metrics. This includes impression,
// abandonment, and launch information reported by the AppListNotifier.
class SearchMetricsManager : ash::AppListNotifier::Observer {
 public:
  using Result = ash::AppListNotifier::Result;
  using Location = ash::AppListNotifier::Location;

  // Represents the actions a user can take in the launcher. These values
  // persist
  // to logs. Entries should not be renumbered and numeric values should never
  // be reused.
  enum class Action {
    kImpression = 0,
    kLaunch = 1,
    kAbandon = 2,
    kIgnore = 3,
    kMaxValue = kIgnore
  };

  SearchMetricsManager(Profile* profile, ash::AppListNotifier* notifier);
  ~SearchMetricsManager() override;

  SearchMetricsManager(const SearchMetricsManager&) = delete;
  SearchMetricsManager& operator=(const SearchMetricsManager&) = delete;

  // ash::AppListNotifier::Observer:
  void OnImpression(Location location,
                    const std::vector<Result>& results,
                    const std::u16string& query) override;
  void OnAbandon(Location location,
                 const std::vector<Result>& results,
                 const std::u16string& query) override;
  void OnLaunch(Location location,
                const Result& launched,
                const std::vector<Result>& shown,
                const std::u16string& query) override;
  void OnIgnore(Location location,
                const std::vector<Result>& results,
                const std::u16string& query) override;

 private:
  base::ScopedObservation<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observation_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_METRICS_MANAGER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/scoped_observation.h"

namespace app_list {

// Records launcher search backend federated analytics.
class FederatedMetricsManager : ash::AppListNotifier::Observer {
 public:
  using Result = ash::AppListNotifier::Result;
  using Location = ash::AppListNotifier::Location;

  explicit FederatedMetricsManager(ash::AppListNotifier* notifier);
  ~FederatedMetricsManager() override;

  FederatedMetricsManager(const FederatedMetricsManager&) = delete;
  FederatedMetricsManager& operator=(const FederatedMetricsManager&) = delete;

  // ash::AppListNotifier::Observer:
  void OnAbandon(Location location,
                 const std::vector<Result>& results,
                 const std::u16string& query) override;
  void OnLaunch(Location location,
                const Result& launched,
                const std::vector<Result>& shown,
                const std::u16string& query) override;

 private:
  base::ScopedObservation<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observation_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_

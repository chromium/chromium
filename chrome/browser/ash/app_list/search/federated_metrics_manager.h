// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/system/federated/federated_service_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace app_list::federated {

constexpr char kHistogramAction[] = "Apps.AppList.Search.Federated.Action";
constexpr char kHistogramInitStatus[] =
    "Apps.AppList.Search.Federated.InitStatus";
constexpr char kHistogramReportStatus[] =
    "Apps.AppList.Search.Federated.ReportStatus";

// Records launcher search backend federated analytics.
class FederatedMetricsManager : ash::AppListNotifier::Observer {
 public:
  using Result = ash::AppListNotifier::Result;
  using Location = ash::AppListNotifier::Location;

  // Represents the actions a user can take in the launcher. These values
  // persist to logs. Entries should not be renumbered and numeric values should
  // never be reused.
  enum class Action {
    kImpression = 0,
    kLaunch = 1,
    kAbandon = 2,
    kIgnore = 3,
    kMaxValue = kIgnore
  };

  // Represents possible states in the initialization of the federated metrics
  // manager itself. These values persist to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class InitStatus {
    kOk = 0,
    kMissingNotifier = 1,
    kMissingController = 2,
    kFederatedConnectionFailedToEstablish = 3,
    kMaxValue = kFederatedConnectionFailedToEstablish
  };

  // Represents possible states in the example reporting of the federated
  // metrics manager itself. These values persist to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class ReportStatus {
    kOk = 0,
    kFederatedServiceNotAvailable = 1,
    kFederatedServiceNotConnected = 2,
    kMaxValue = kFederatedServiceNotConnected
  };

  FederatedMetricsManager(
      ash::AppListNotifier* notifier,
      ash::federated::FederatedServiceController* controller);
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
  // Note: There's no guarantee that the federated service will stay
  // available, so call `IsFederatedServiceAvailable()` before each attempt at
  // interacting with the service.
  bool IsFederatedServiceAvailable();
  void TryToBindFederatedServiceIfNecessary();
  void LogExample(const std::string& example_str);

  base::ScopedObservation<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observation_{this};
  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service_;
  const raw_ptr<ash::federated::FederatedServiceController> controller_;
};

}  // namespace app_list::federated

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_

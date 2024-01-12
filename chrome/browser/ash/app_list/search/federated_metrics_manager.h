// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/system/federated/federated_service_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace app_list::federated {

inline constexpr char kHistogramSearchSessionConclusion[] =
    "Apps.AppList.Search.Federated.SearchSessionConclusion";
inline constexpr char kHistogramInitStatus[] =
    "Apps.AppList.Search.Federated.InitStatus";
inline constexpr char kHistogramQueryLengthOnStorageSuccess[] =
    "Apps.AppList.Search.Federated.QueryLengthOnStorageSuccess";
inline constexpr char kHistogramReportStatus[] =
    "Apps.AppList.Search.Federated.ReportStatus";

const int kMaxLoggedQueryLengthOnStorageSuccess = 20;

// Records launcher search backend federated analytics.
// Requires that OnDefaultSearchIsGoogleSet() is called after class creation and
// before any logging is triggered. Enforced by a CHECK().
//
// TODO(b/289140140): Consider migrating to FederatedClientManager utility class
// after that class is completed.
class FederatedMetricsManager : ash::AppListNotifier::Observer {
 public:
  using Result = ash::AppListNotifier::Result;
  using Location = ash::AppListNotifier::Location;

  // Represents the actions a user can take in the launcher. These values
  // persist to logs. Entries should not be renumbered and numeric values should
  // never be reused.
  // TODO(b/262611120): Currently unused. Deprecate if remains unused.
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
  // TODO(b/262611120): `FederatedMetricsManager` tracks session-level actions
  // by monitoring finer-grained events. Certain orderings of events are assumed
  // by `FederatedMetricsManager`, but are not strictly guaranteed by the
  // `AppListNotifier`. For example, that `OnLaunch` is followed by
  // `OnSearchSessionEnded`. Consider adding new methods to the
  // `AppListNotifier` which more directly signal session-level events of
  // interest.
  void OnSearchSessionStarted() override;
  void OnSearchSessionEnded(const std::u16string& query) override;
  void OnSeen(Location location,
              const std::vector<Result>& results,
              const std::u16string& query) override;
  void OnLaunch(Location location,
                const Result& launched,
                const std::vector<Result>& shown,
                const std::u16string& query) override;

  // Default search engine is one criterion for determining whether logging
  // will proceed.
  // First called via AppListClientImpl::SetProfile(), and subsequently via
  // AppListClientImpl::OnTemplateURLServiceChanged().
  void OnDefaultSearchIsGoogleSet(bool is_google);

 private:
  // Whether the metrics manager is tracking an active search session.
  bool session_active_ = false;
  // Tracks the metric recorded when EndSearchSession() is called.
  ash::SearchSessionConclusion session_result_ =
      ash::SearchSessionConclusion::kQuit;

  std::optional<bool> is_default_search_engine_google_;

  // Note: There's no guarantee that the federated service will stay
  // available, so call `IsFederatedServiceAvailable()` before each attempt at
  // interacting with the service.
  bool IsFederatedServiceAvailable();
  bool IsLoggingEnabled();
  void TryToBindFederatedServiceIfNecessary();
  void LogExample(const std::string& query);

  base::ScopedObservation<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observation_{this};
  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service_;
  const raw_ptr<ash::federated::FederatedServiceController> controller_;
};

}  // namespace app_list::federated

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FEDERATED_METRICS_MANAGER_H_

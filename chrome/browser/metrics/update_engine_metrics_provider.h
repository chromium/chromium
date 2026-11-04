// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_UPDATE_ENGINE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_UPDATE_ENGINE_METRICS_PROVIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class UpdateEngineMetricsProvider
    : public metrics::MetricsProvider,
      public session_manager::SessionManagerObserver,
      public signin::IdentityManager::Observer {
 public:
  // `session_manager` may be nullptr in unit tests.
  explicit UpdateEngineMetricsProvider(
      session_manager::SessionManager* session_manager);

  UpdateEngineMetricsProvider(const UpdateEngineMetricsProvider&) = delete;
  UpdateEngineMetricsProvider& operator=(const UpdateEngineMetricsProvider&) =
      delete;

  ~UpdateEngineMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto_unused) override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  bool IsConsumerAutoUpdateToggleEligible();

  const raw_ptr<session_manager::SessionManager> session_manager_;
  // `ProvideCurrentSessionData()` may be called after the user session is
  // created in `SessionManager` but before the user profile is loaded, or
  // during shutdown after the profile and its `IdentityManager` are destroyed.
  // `IdentityManagerProvider::Find()` requires the profile to be loaded, so
  // track live `IdentityManager` instances across profile load and shutdown.
  std::map<AccountId, raw_ptr<signin::IdentityManager>> identity_managers_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
  base::ScopedMultiSourceObservation<signin::IdentityManager,
                                     signin::IdentityManager::Observer>
      identity_manager_observations_{this};
};

#endif  // CHROME_BROWSER_METRICS_UPDATE_ENGINE_METRICS_PROVIDER_H_

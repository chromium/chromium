// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "chrome/browser/enterprise/connectors/analysis_service_settings.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting_service_settings.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Manages access to Connector policies for a given profile. This class is
// responsible for caching the Connector policies, validate them against
// approved service providers and provide a simple interface to them.
class ConnectorsManager {
 public:
  // Maps used to cache connectors settings.
  using AnalysisConnectorsSettings =
      std::map<AnalysisConnector, std::vector<AnalysisServiceSettings>>;
  using ReportingConnectorsSettings =
      std::map<ReportingConnector, std::vector<ReportingServiceSettings>>;

  ConnectorsManager(PrefService* pref_service,
                    ServiceProviderConfig* config,
                    bool observe_prefs = true);
  ~ConnectorsManager();

  // Validates which settings should be applied to a reporting event
  // against cached policies. Cache the policy value the first time this is
  // called for every different connector.
  base::Optional<ReportingSettings> GetReportingSettings(
      ReportingConnector connector);

  // Validates which settings should be applied to an analysis connector event
  // against cached policies. This function will prioritize new connector
  // policies over legacy ones if they are set.
  base::Optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      AnalysisConnector connector);

  // Checks if the corresponding connector is enabled.
  bool IsConnectorEnabled(AnalysisConnector connector) const;
  bool IsConnectorEnabled(ReportingConnector connector) const;

  bool DelayUntilVerdict(AnalysisConnector connector);

  // Public testing functions.
  const AnalysisConnectorsSettings& GetAnalysisConnectorsSettingsForTesting()
      const;
  const ReportingConnectorsSettings& GetReportingConnectorsSettingsForTesting()
      const;

 private:
  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  base::Optional<AnalysisSettings> GetAnalysisSettingsFromConnectorPolicy(
      const GURL& url,
      AnalysisConnector connector);

  // Read and cache the policy corresponding to |connector|.
  void CacheAnalysisConnectorPolicy(AnalysisConnector connector);
  void CacheReportingConnectorPolicy(ReportingConnector connector);

  // Sets up |pref_change_registrar_|. Used by the constructor and
  // SetUpForTesting.
  void StartObservingPrefs(PrefService* pref_service);
  void StartObservingPref(AnalysisConnector connector);
  void StartObservingPref(ReportingConnector connector);

  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  base::Optional<ReportingSettings> GetReportingSettingsFromConnectorPolicy(
      ReportingConnector connector);

  // Cached values of available service providers. This information validates
  // the Connector policies have a valid provider.
  ServiceProviderConfig* service_provider_config_;

  // Cached values of the connector policies. Updated when a connector is first
  // used or when a policy is updated.
  AnalysisConnectorsSettings analysis_connector_settings_;
  ReportingConnectorsSettings reporting_connector_settings_;

  // Used to track changes of connector policies and propagate them in
  // |connector_settings_|.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include <set>

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "chrome/browser/enterprise/connectors/analysis_service_settings.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting_service_settings.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/gurl.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace enterprise_connectors {

// Controls whether the Enterprise Connectors policies should be read by
// ConnectorsManager. Legacy policies will be read as a fallback if this feature
// is disabled.
extern const base::Feature kEnterpriseConnectorsEnabled;

// For the moment, service provider configurations are static and only support
// google endpoints.  Therefore the configurtion is placed here directly.
// Once the configuation becomes more dynamic this static string will be
// removed and replaced with a service to keep it up to date.
extern const char kServiceProviderConfig[];

// Manages access to Connector policies. This class is responsible for caching
// the Connector policies, validate them against approved service providers and
// provide a simple interface to them.
class ConnectorsManager {
 public:
  // Maps used to cache connectors settings.
  using AnalysisConnectorsSettings =
      std::map<AnalysisConnector, std::vector<AnalysisServiceSettings>>;
  using ReportingConnectorsSettings =
      std::map<ReportingConnector, std::vector<ReportingServiceSettings>>;

  static ConnectorsManager* GetInstance();

  // Validates which settings should be applied to a reporting event
  // against cached policies. This function will prioritize new connector
  // policies over legacy ones if they are set.
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

  // Helpers to reset the ConnectorManager instance across test since it would
  // otherwise persist its state.
  void SetUpForTesting();
  void TearDownForTesting();
  void ClearCacheForTesting();

 private:
  friend class base::NoDestructor<ConnectorsManager>;

  // Constructor and destructor are declared as private so callers use
  // GetInstance instead.
  ConnectorsManager();
  ~ConnectorsManager();

  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  base::Optional<AnalysisSettings> GetAnalysisSettingsFromConnectorPolicy(
      const GURL& url,
      AnalysisConnector connector);

  // Read and cache the policy corresponding to |connector|.
  void CacheAnalysisConnectorPolicy(AnalysisConnector connector);
  void CacheReportingConnectorPolicy(ReportingConnector connector);

  // Sets up |pref_change_registrar_| if kEnterpriseConntorsEnabled is true.
  // Used by the constructor and SetUpForTesting.
  void StartObservingPrefs();
  void StartObservingPref(AnalysisConnector connector);
  void StartObservingPref(ReportingConnector connector);

  // Private legacy functions.
  // These functions are used to interact with legacy policies and should stay
  // private. They should be removed once legacy policies are deprecated.

  // Returns analysis settings based on legacy policies.
  base::Optional<AnalysisSettings> GetAnalysisSettingsFromLegacyPolicies(
      const GURL& url,
      AnalysisConnector connector) const;

  BlockUntilVerdict LegacyBlockUntilVerdict(bool upload) const;
  bool LegacyBlockPasswordProtectedFiles(bool upload) const;
  bool LegacyBlockLargeFiles(bool upload) const;
  bool LegacyBlockUnsupportedFileTypes(bool upload) const;

  // Functions that check a url against the corresponding URL patterns policies.
  bool MatchURLAgainstLegacyDlpPolicies(const GURL& url, bool upload) const;
  bool MatchURLAgainstLegacyMalwarePolicies(const GURL& url, bool upload) const;
  std::set<std::string> MatchURLAgainstLegacyPolicies(const GURL& url,
                                                      bool upload) const;

  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  base::Optional<ReportingSettings> GetReportingSettingsFromConnectorPolicy(
      ReportingConnector connector);

  // Returns reporting settings based on legacy policies.
  base::Optional<ReportingSettings> GetReportingSettingsFromLegacyPolicies(
      ReportingConnector connector) const;

  // Cached values of available service providers. This information validates
  // the Connector policies have a valid provider.
  ServiceProviderConfig service_provider_config_ =
      ServiceProviderConfig(kServiceProviderConfig);

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

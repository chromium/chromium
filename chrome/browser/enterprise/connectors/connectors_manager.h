// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {
class BrowserCrashEventRouter;

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

  ConnectorsManager(
      std::unique_ptr<BrowserCrashEventRouter> browser_crash_event_router,
      std::unique_ptr<ExtensionInstallEventRouter> extension_install_router,
      PrefService* pref_service,
      const ServiceProviderConfig* config,
      bool observe_prefs = true);
  ~ConnectorsManager();

  // Validates which settings should be applied to a reporting event
  // against cached policies. Cache the policy value the first time this is
  // called for every different connector.
  absl::optional<ReportingSettings> GetReportingSettings(
      ReportingConnector connector);

  // Validates which settings should be applied to an analysis connector event
  // against cached policies. This function will prioritize new connector
  // policies over legacy ones if they are set.
  absl::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      AnalysisConnector connector);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  absl::optional<AnalysisSettings> GetAnalysisSettings(
      content::BrowserContext* context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      AnalysisConnector connector);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Checks if the corresponding connector is enabled.
  bool IsConnectorEnabled(AnalysisConnector connector) const;
  bool IsConnectorEnabled(ReportingConnector connector) const;

  bool DelayUntilVerdict(AnalysisConnector connector);
  absl::optional<std::u16string> GetCustomMessage(AnalysisConnector connector,
                                                  const std::string& tag);
  absl::optional<GURL> GetLearnMoreUrl(AnalysisConnector connector,
                                       const std::string& tag);
  bool GetBypassJustificationRequired(AnalysisConnector connector,
                                      const std::string& tag);

  std::vector<std::string> GetAnalysisServiceProviderNames(
      AnalysisConnector connector);
  std::vector<std::string> GetReportingServiceProviderNames(
      ReportingConnector connector);

  std::vector<const AnalysisConfig*> GetAnalysisServiceConfigs(
      AnalysisConnector connector);

  // Public testing functions.
  const AnalysisConnectorsSettings& GetAnalysisConnectorsSettingsForTesting()
      const;
  const ReportingConnectorsSettings& GetReportingConnectorsSettingsForTesting()
      const;

 private:
  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  absl::optional<AnalysisSettings> GetAnalysisSettingsFromConnectorPolicy(
      const GURL& url,
      AnalysisConnector connector);

  // Read and cache the policy corresponding to |connector|.
  void CacheAnalysisConnectorPolicy(AnalysisConnector connector) const;
  void CacheReportingConnectorPolicy(ReportingConnector connector);

  // Sets up |pref_change_registrar_|. Used by the constructor and
  // SetUpForTesting.
  void StartObservingPrefs(PrefService* pref_service);
  void StartObservingPref(AnalysisConnector connector);
  void StartObservingPref(ReportingConnector connector);

  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  absl::optional<ReportingSettings> GetReportingSettingsFromConnectorPolicy(
      ReportingConnector connector);

  PrefService* prefs() { return pref_change_registrar_.prefs(); }

  const PrefService* prefs() const { return pref_change_registrar_.prefs(); }

  // Cached values of available service providers. This information validates
  // the Connector policies have a valid provider.
  raw_ptr<const ServiceProviderConfig> service_provider_config_;

  // Cached values of the connector policies. Updated when a connector is first
  // used or when a policy is updated.  Analysis connectors settings are
  // mutable because they maybe updated by a call to IsConnectorEnabled(),
  // which is a const method.
  mutable AnalysisConnectorsSettings analysis_connector_settings_;
  ReportingConnectorsSettings reporting_connector_settings_;

  // Used to track changes of connector policies and propagate them in
  // |connector_settings_|.
  PrefChangeRegistrar pref_change_registrar_;

  // A router to report browser crash events via the reporting pipeline.
  std::unique_ptr<BrowserCrashEventRouter> browser_crash_event_router_;

  // An observer to report extension install events via the reporting pipeline.
  std::unique_ptr<ExtensionInstallEventRouter> extension_install_event_router_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

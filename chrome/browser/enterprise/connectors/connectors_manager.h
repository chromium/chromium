// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/browser/browser_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

// Manages access to Connector policies for a given profile. This class is
// responsible for caching the Connector policies, validate them against
// approved service providers and provide a simple interface to them.
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
class ConnectorsManager : public BrowserListObserver,
                          public TabStripModelObserver {
#else
class ConnectorsManager {
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

 public:
  // Maps used to cache connectors settings.
  using AnalysisConnectorsSettings =
      std::map<AnalysisConnector, std::vector<AnalysisServiceSettings>>;
  using ReportingConnectorsSettings =
      std::map<ReportingConnector, std::vector<ReportingServiceSettings>>;

  ConnectorsManager(PrefService* pref_service,
                    const ServiceProviderConfig* config,
                    bool observe_prefs = true);
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  ~ConnectorsManager() override;
#else
  ~ConnectorsManager();
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  // Validates which settings should be applied to a reporting event
  // against cached policies. Cache the policy value the first time this is
  // called for every different connector.
  std::optional<ReportingSettings> GetReportingSettings(
      ReportingConnector connector);

  // Validates which settings should be applied to an analysis connector event
  // against cached policies. This function will prioritize new connector
  // policies over legacy ones if they are set.
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      AnalysisConnector connector);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<AnalysisSettings> GetAnalysisSettings(
      content::BrowserContext* context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      AnalysisConnector connector);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Checks if the corresponding connector is enabled.
  bool IsConnectorEnabled(AnalysisConnector connector) const;
  bool IsConnectorEnabled(ReportingConnector connector) const;

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // Check if the corresponding connector is enabled for any local agent.
  bool IsConnectorEnabledForLocalAgent(AnalysisConnector connector) const;
#endif

  bool DelayUntilVerdict(AnalysisConnector connector);
  std::optional<std::u16string> GetCustomMessage(AnalysisConnector connector,
                                                 const std::string& tag);
  std::optional<GURL> GetLearnMoreUrl(AnalysisConnector connector,
                                      const std::string& tag);
  bool GetBypassJustificationRequired(AnalysisConnector connector,
                                      const std::string& tag);

  std::vector<std::string> GetAnalysisServiceProviderNames(
      AnalysisConnector connector);
  std::vector<std::string> GetReportingServiceProviderNames(
      ReportingConnector connector);

  std::vector<const AnalysisConfig*> GetAnalysisServiceConfigs(
      AnalysisConnector connector);

  void SetTelemetryObserverCallback(base::RepeatingCallback<void()> callback);

  // Public testing functions.
  const AnalysisConnectorsSettings& GetAnalysisConnectorsSettingsForTesting()
      const;
  const ReportingConnectorsSettings& GetReportingConnectorsSettingsForTesting()
      const;
  const base::RepeatingCallback<void()> GetTelemetryObserverCallbackForTesting()
      const;

 private:
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  // Read and cache the policy corresponding to |connector|.
  void CacheAnalysisConnectorPolicy(AnalysisConnector connector) const;
  void CacheReportingConnectorPolicy(ReportingConnector connector);

  // Get data location region from policy.
  DataRegion GetDataRegion() const;

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // Close connection with local agent if all the relevant connectors are turned
  // off for it.
  void MaybeCloseLocalContentAnalysisAgentConnection();
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  // Re-cache analysis connector policy and update local agent connection if
  // needed.
  void OnPrefChanged(AnalysisConnector connector);
  void OnPrefChanged(ReportingConnector connector);

  // Sets up |pref_change_registrar_|. Used by the constructor and
  // SetUpForTesting.
  void StartObservingPrefs(PrefService* pref_service);
  void StartObservingPref(AnalysisConnector connector);
  void StartObservingPref(ReportingConnector connector);

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

  // Used to report changes of reporting connector policy.
  base::RepeatingCallback<void()> telemetry_observer_callback_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

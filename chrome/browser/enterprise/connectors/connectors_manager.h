// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  using FileSystemConnectorsSettings =
      std::map<FileSystemConnector, std::vector<FileSystemServiceSettings>>;

  ConnectorsManager(PrefService* pref_service,
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

  // Validates which settings should be applied to a file system connector
  // against cached policies.
  absl::optional<FileSystemSettings> GetFileSystemGlobalSettings(
      FileSystemConnector connector);
  // In addition to method above; also validates specifically for an URL.
  absl::optional<FileSystemSettings> GetFileSystemSettings(
      const GURL& url,
      FileSystemConnector connector);

  // Checks if the corresponding connector is enabled.
  bool IsConnectorEnabled(AnalysisConnector connector) const;
  bool IsConnectorEnabled(ReportingConnector connector) const;
  bool IsConnectorEnabled(FileSystemConnector connector) const;

  bool DelayUntilVerdict(AnalysisConnector connector);
  absl::optional<std::u16string> GetCustomMessage(AnalysisConnector connector,
                                                  const std::string& tag);
  absl::optional<GURL> GetLearnMoreUrl(AnalysisConnector connector,
                                       const std::string& tag);
  absl::optional<bool> GetBypassJustificationRequired(
      AnalysisConnector connector,
      const std::string& tag);

  std::vector<std::string> GetAnalysisServiceProviderNames(
      AnalysisConnector connector);
  std::vector<std::string> GetReportingServiceProviderNames(
      ReportingConnector connector);

  // Public testing functions.
  const AnalysisConnectorsSettings& GetAnalysisConnectorsSettingsForTesting()
      const;
  const ReportingConnectorsSettings& GetReportingConnectorsSettingsForTesting()
      const;
  const FileSystemConnectorsSettings&
  GetFileSystemConnectorsSettingsForTesting() const;

 private:
  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  absl::optional<AnalysisSettings> GetAnalysisSettingsFromConnectorPolicy(
      const GURL& url,
      AnalysisConnector connector);

  // Read and cache the policy corresponding to |connector|.
  void CacheAnalysisConnectorPolicy(AnalysisConnector connector);
  void CacheReportingConnectorPolicy(ReportingConnector connector);
  void CacheFileSystemConnectorPolicy(FileSystemConnector connector);

  // Sets up |pref_change_registrar_|. Used by the constructor and
  // SetUpForTesting.
  void StartObservingPrefs(PrefService* pref_service);
  void StartObservingPref(AnalysisConnector connector);
  void StartObservingPref(ReportingConnector connector);
  void StartObservingPref(FileSystemConnector connector);

  // Validates which settings should be applied to an analysis connector event
  // against connector policies. Cache the policy value the first time this is
  // called for every different connector.
  absl::optional<ReportingSettings> GetReportingSettingsFromConnectorPolicy(
      ReportingConnector connector);

  // Returns service settings (if there are multiple service providers, only the
  // first one for now) for |connector|. Cache the policy value the first time
  // this is called for every different connector.
  FileSystemServiceSettings* GetFileSystemServiceSettings(
      FileSystemConnector connector);

  // Cached values of available service providers. This information validates
  // the Connector policies have a valid provider.
  raw_ptr<const ServiceProviderConfig> service_provider_config_;

  // Cached values of the connector policies. Updated when a connector is first
  // used or when a policy is updated.
  AnalysisConnectorsSettings analysis_connector_settings_;
  ReportingConnectorsSettings reporting_connector_settings_;
  FileSystemConnectorsSettings file_system_connector_settings_;

  // Used to track changes of connector policies and propagate them in
  // |connector_settings_|.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

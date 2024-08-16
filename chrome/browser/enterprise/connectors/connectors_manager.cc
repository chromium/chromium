// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "url/gurl.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"  // nogncheck
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
static constexpr enterprise_connectors::AnalysisConnector
    kLocalAnalysisConnectors[] = {
        AnalysisConnector::BULK_DATA_ENTRY,
        AnalysisConnector::FILE_ATTACHED,
        AnalysisConnector::PRINT,
};
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

constexpr char kReportingConnectorUrlFlag[] = "reporting-connector-url";

std::optional<GURL> GetReportingConnectorUrlOverride() {
  // Ignore this flag on Stable and Beta to avoid abuse.
  if (!g_browser_process || !g_browser_process->browser_policy_connector()
                                 ->IsCommandLineSwitchSupported()) {
    return std::nullopt;
  }

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(kReportingConnectorUrlFlag)) {
    GURL url = GURL(cmd->GetSwitchValueASCII(kReportingConnectorUrlFlag));
    if (url.is_valid()) {
      return url;
    } else {
      VLOG(1) << "--" << kReportingConnectorUrlFlag
              << " is set to an invalid URL";
    }
  }

  return std::nullopt;
}

}  // namespace

ConnectorsManager::ConnectorsManager(PrefService* pref_service,
                                     const ServiceProviderConfig* config,
                                     bool observe_prefs)
    : service_provider_config_(config) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // Start observing tab strip models for all browsers.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    OnBrowserAdded(browser);
  }
  browser_list->AddObserver(this);
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  if (observe_prefs) {
    StartObservingPrefs(pref_service);
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
    MaybeCloseLocalContentAnalysisAgentConnection();
#endif
  }
}

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
ConnectorsManager::~ConnectorsManager() {
  BrowserList* browser_list = BrowserList::GetInstance();
  browser_list->RemoveObserver(this);
  for (Browser* browser : *browser_list) {
    OnBrowserRemoved(browser);
  }
}
#else
ConnectorsManager::~ConnectorsManager() = default;
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

bool ConnectorsManager::IsConnectorEnabled(AnalysisConnector connector) const {
  if (analysis_connector_settings_.count(connector) == 0 &&
      prefs()->HasPrefPath(ConnectorPref(connector))) {
    CacheAnalysisConnectorPolicy(connector);
  }

  return analysis_connector_settings_.count(connector);
}

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
bool ConnectorsManager::IsConnectorEnabledForLocalAgent(
    AnalysisConnector connector) const {
  if (!IsConnectorEnabled(connector)) {
    return false;
  }
  return analysis_connector_settings_.at(connector)[0].is_local_analysis();
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

bool ConnectorsManager::IsConnectorEnabled(ReportingConnector connector) const {
  if (reporting_connector_settings_.count(connector) == 1)
    return true;

  const char* pref = ConnectorPref(connector);
  return pref && prefs()->HasPrefPath(pref);
}

std::optional<ReportingSettings> ConnectorsManager::GetReportingSettings(
    ReportingConnector connector) {
  if (!IsConnectorEnabled(connector))
    return std::nullopt;

  if (reporting_connector_settings_.count(connector) == 0)
    CacheReportingConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (reporting_connector_settings_.count(connector) == 0)
    return std::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  auto reporting_settings =
      reporting_connector_settings_[connector][0].GetReportingSettings();

  std::optional<GURL> url_override = GetReportingConnectorUrlOverride();
  if (reporting_settings && url_override) {
    reporting_settings->reporting_url = std::move(*url_override);
  }

  return reporting_settings;
}

std::optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!IsConnectorEnabled(connector))
    return std::nullopt;

  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return std::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0].GetAnalysisSettings(
      url, GetDataRegion());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    content::BrowserContext* context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    AnalysisConnector connector) {
  if (!IsConnectorEnabled(connector))
    return std::nullopt;

  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return std::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0].GetAnalysisSettings(
      context, source_url, destination_url, GetDataRegion());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
void ConnectorsManager::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void ConnectorsManager::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}

void ConnectorsManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Checking only when new tab is open.
  if (change.type() != TabStripModelChange::kInserted) {
    return;
  }

  for (auto connector : kLocalAnalysisConnectors) {
    if (!IsConnectorEnabledForLocalAgent(connector)) {
      continue;
    }

    // Send a connection event to the local agent. If all the enabled connectors
    // are configured to use the same agent, the same connection is reused here.
    auto configs = GetAnalysisServiceConfigs(connector);
    enterprise_connectors::ContentAnalysisSdkManager::Get()->GetClient(
        {configs[0]->local_path, configs[0]->user_specific});
  }
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) const {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::Value::List& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value)
    analysis_connector_settings_[connector].emplace_back(
        service_settings, *service_provider_config_);
}

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
void ConnectorsManager::MaybeCloseLocalContentAnalysisAgentConnection() {
  for (auto connector : kLocalAnalysisConnectors) {
    if (IsConnectorEnabledForLocalAgent(connector)) {
      // Return early because at lease one access point is enabled for local
      // agent.
      return;
    }
  }
  // Delete connection with local agents when no access point is enabled.
  ContentAnalysisSdkManager::Get()->ResetAllClients();
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

void ConnectorsManager::SetTelemetryObserverCallback(
    base::RepeatingCallback<void()> callback) {
  telemetry_observer_callback_ = callback;
}

void ConnectorsManager::OnPrefChanged(AnalysisConnector connector) {
  CacheAnalysisConnectorPolicy(connector);
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  MaybeCloseLocalContentAnalysisAgentConnection();
#endif
}

void ConnectorsManager::OnPrefChanged(ReportingConnector connector) {
  CacheReportingConnectorPolicy(connector);
  if (!telemetry_observer_callback_.is_null()) {
    telemetry_observer_callback_.Run();
  }
}

void ConnectorsManager::CacheReportingConnectorPolicy(
    ReportingConnector connector) {
  reporting_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::Value::List& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    reporting_connector_settings_[connector].emplace_back(
        service_settings, *service_provider_config_);
  }
}

bool ConnectorsManager::DelayUntilVerdict(AnalysisConnector connector) {
  if (IsConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0)
      CacheAnalysisConnectorPolicy(connector);

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector)
          .at(0)
          .ShouldBlockUntilVerdict();
    }
  }
  return false;
}

std::optional<std::u16string> ConnectorsManager::GetCustomMessage(
    AnalysisConnector connector,
    const std::string& tag) {
  if (IsConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0)
      CacheAnalysisConnectorPolicy(connector);

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector).at(0).GetCustomMessage(
          tag);
    }
  }
  return std::nullopt;
}

std::optional<GURL> ConnectorsManager::GetLearnMoreUrl(
    AnalysisConnector connector,
    const std::string& tag) {
  if (IsConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0)
      CacheAnalysisConnectorPolicy(connector);

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector).at(0).GetLearnMoreUrl(
          tag);
    }
  }
  return std::nullopt;
}

bool ConnectorsManager::GetBypassJustificationRequired(
    AnalysisConnector connector,
    const std::string& tag) {
  if (IsConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0)
      CacheAnalysisConnectorPolicy(connector);

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector)
          .at(0)
          .GetBypassJustificationRequired(tag);
    }
  }
  return false;
}

std::vector<std::string> ConnectorsManager::GetAnalysisServiceProviderNames(
    AnalysisConnector connector) {
  if (IsConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      // There can only be one provider right now, but the system is designed to
      // support multiples, so return a vector.
      return {analysis_connector_settings_.at(connector)
                  .at(0)
                  .service_provider_name()};
    }
  }

  return {};
}

std::vector<std::string> ConnectorsManager::GetReportingServiceProviderNames(
    ReportingConnector connector) {
  if (!IsConnectorEnabled(connector))
    return {};

  if (reporting_connector_settings_.count(connector) == 0)
    CacheReportingConnectorPolicy(connector);

  if (reporting_connector_settings_.count(connector) &&
      !reporting_connector_settings_.at(connector).empty()) {
    // There can only be one provider right now, but the system is designed to
    // support multiples, so return a vector.
    return {reporting_connector_settings_.at(connector)
                .at(0)
                .service_provider_name()};
  }

  return {};
}

std::vector<const AnalysisConfig*> ConnectorsManager::GetAnalysisServiceConfigs(
    AnalysisConnector connector) {
  if (IsConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      // There can only be one provider right now, but the system is designed to
      // support multiples, so return a vector.
      return {
          analysis_connector_settings_.at(connector).at(0).GetAnalysisConfig()};
    }
  }

  return {};
}

DataRegion ConnectorsManager::GetDataRegion() const {
#if BUILDFLAG(IS_ANDROID)
  return DataRegion::NO_PREFERENCE;
#else
  bool apply_data_region =
      prefs()->HasPrefPath(prefs::kChromeDataRegionSetting) &&
      base::FeatureList::IsEnabled(safe_browsing::kDlpRegionalizedEndpoints);
  return apply_data_region ? ChromeDataRegionSettingToEnum(prefs()->GetInteger(
                                 prefs::kChromeDataRegionSetting))
                           : DataRegion::NO_PREFERENCE;
#endif
}

void ConnectorsManager::StartObservingPrefs(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  StartObservingPref(AnalysisConnector::FILE_ATTACHED);
  StartObservingPref(AnalysisConnector::FILE_DOWNLOADED);
  StartObservingPref(AnalysisConnector::BULK_DATA_ENTRY);
  StartObservingPref(AnalysisConnector::PRINT);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  StartObservingPref(AnalysisConnector::FILE_TRANSFER);
#endif
  StartObservingPref(ReportingConnector::SECURITY_EVENT);
}

void ConnectorsManager::StartObservingPref(AnalysisConnector connector) {
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref, base::BindRepeating(
                  static_cast<void (ConnectorsManager::*)(AnalysisConnector)>(
                      &ConnectorsManager::OnPrefChanged),
                  base::Unretained(this), connector));
  }
}

void ConnectorsManager::StartObservingPref(ReportingConnector connector) {
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref, base::BindRepeating(
                  static_cast<void (ConnectorsManager::*)(ReportingConnector)>(
                      &ConnectorsManager::OnPrefChanged),
                  base::Unretained(this), connector));
  }
}

const ConnectorsManager::AnalysisConnectorsSettings&
ConnectorsManager::GetAnalysisConnectorsSettingsForTesting() const {
  return analysis_connector_settings_;
}

const ConnectorsManager::ReportingConnectorsSettings&
ConnectorsManager::GetReportingConnectorsSettingsForTesting() const {
  return reporting_connector_settings_;
}

const base::RepeatingCallback<void()>
ConnectorsManager::GetTelemetryObserverCallbackForTesting() const {
  return telemetry_observer_callback_;
}

}  // namespace enterprise_connectors

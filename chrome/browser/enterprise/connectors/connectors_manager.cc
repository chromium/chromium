// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include <memory>

#include "base/check.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/reporting/browser_crash_event_router.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace enterprise_connectors {

ConnectorsManager::ConnectorsManager(
    std::unique_ptr<BrowserCrashEventRouter> browser_crash_event_router,
    std::unique_ptr<ExtensionInstallEventRouter> extension_install_event_router,
    PrefService* pref_service,
    const ServiceProviderConfig* config,
    bool observe_prefs)
    : service_provider_config_(config),
      browser_crash_event_router_(std::move(browser_crash_event_router)),
      extension_install_event_router_(
          std::move(extension_install_event_router)) {
  DCHECK(browser_crash_event_router_) << "Crash event router is null";
  DCHECK(extension_install_event_router_) << "Extension event router is null";
  if (observe_prefs)
    StartObservingPrefs(pref_service);
  extension_install_event_router_->StartObserving();
}

ConnectorsManager::~ConnectorsManager() = default;

bool ConnectorsManager::IsConnectorEnabled(AnalysisConnector connector) const {
  if (analysis_connector_settings_.count(connector) == 1)
    return true;

  const char* pref = ConnectorPref(connector);
  return pref && pref_change_registrar_.prefs()->HasPrefPath(pref);
}

bool ConnectorsManager::IsConnectorEnabled(ReportingConnector connector) const {
  if (reporting_connector_settings_.count(connector) == 1)
    return true;

  const char* pref = ConnectorPref(connector);
  return pref && pref_change_registrar_.prefs()->HasPrefPath(pref);
}

absl::optional<ReportingSettings> ConnectorsManager::GetReportingSettings(
    ReportingConnector connector) {
  if (!IsConnectorEnabled(connector))
    return absl::nullopt;

  if (reporting_connector_settings_.count(connector) == 0)
    CacheReportingConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (reporting_connector_settings_.count(connector) == 0)
    return absl::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return reporting_connector_settings_[connector][0].GetReportingSettings();
}

absl::optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!IsConnectorEnabled(connector))
    return absl::nullopt;

  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return absl::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0].GetAnalysisSettings(url);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    content::BrowserContext* context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    AnalysisConnector connector) {
  if (!IsConnectorEnabled(connector))
    return absl::nullopt;

  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return absl::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0].GetAnalysisSettings(
      context, source_url, destination_url);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

absl::optional<AnalysisSettings>
ConnectorsManager::GetAnalysisSettingsFromConnectorPolicy(
    const GURL& url,
    AnalysisConnector connector) {
  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return absl::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0].GetAnalysisSettings(url);
}

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::Value::List& policy_value =
      pref_change_registrar_.prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value)
    analysis_connector_settings_[connector].emplace_back(
        service_settings, *service_provider_config_);
}

void ConnectorsManager::CacheReportingConnectorPolicy(
    ReportingConnector connector) {
  reporting_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::Value::List& policy_value =
      pref_change_registrar_.prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value)
    reporting_connector_settings_[connector].emplace_back(
        service_settings, *service_provider_config_);
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

absl::optional<std::u16string> ConnectorsManager::GetCustomMessage(
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
  return absl::nullopt;
}

absl::optional<GURL> ConnectorsManager::GetLearnMoreUrl(
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
  return absl::nullopt;
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
        pref,
        base::BindRepeating(&ConnectorsManager::CacheAnalysisConnectorPolicy,
                            base::Unretained(this), connector));
  }
}

void ConnectorsManager::StartObservingPref(ReportingConnector connector) {
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref,
        base::BindRepeating(&ConnectorsManager::CacheReportingConnectorPolicy,
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

}  // namespace enterprise_connectors

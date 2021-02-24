// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"

namespace enterprise_connectors {

ConnectorsManager::ConnectorsManager(PrefService* pref_service,
                                     ServiceProviderConfig* config,
                                     bool observe_prefs)
    : service_provider_config_(config) {
  if (observe_prefs)
    StartObservingPrefs(pref_service);
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

bool ConnectorsManager::IsConnectorEnabled(
    FileSystemConnector connector) const {
  if (file_system_connector_settings_.count(connector) == 1)
    return true;

  const char* pref = ConnectorPref(connector);
  return pref && pref_change_registrar_.prefs()->HasPrefPath(pref);
}

base::Optional<ReportingSettings> ConnectorsManager::GetReportingSettings(
    ReportingConnector connector) {
  if (!IsConnectorEnabled(connector))
    return base::nullopt;

  if (reporting_connector_settings_.count(connector) == 0)
    CacheReportingConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (reporting_connector_settings_.count(connector) == 0)
    return base::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return reporting_connector_settings_[connector][0].GetReportingSettings();
}

base::Optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!IsConnectorEnabled(connector))
    return base::nullopt;

  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return base::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0].GetAnalysisSettings(url);
}

base::Optional<AnalysisSettings>
ConnectorsManager::GetAnalysisSettingsFromConnectorPolicy(
    const GURL& url,
    AnalysisConnector connector) {
  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return base::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0].GetAnalysisSettings(url);
}

base::Optional<FileSystemSettings> ConnectorsManager::GetFileSystemSettings(
    const GURL& url,
    FileSystemConnector connector) {
  if (!IsConnectorEnabled(connector))
    return base::nullopt;

  if (file_system_connector_settings_.count(connector) == 0)
    CacheFileSystemConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (file_system_connector_settings_.count(connector) == 0)
    return base::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return file_system_connector_settings_[connector][0].GetSettings(url);
}

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue* policy_value =
      pref_change_registrar_.prefs()->GetList(pref);
  if (policy_value && policy_value->is_list()) {
    for (const base::Value& service_settings : policy_value->GetList())
      analysis_connector_settings_[connector].emplace_back(
          service_settings, *service_provider_config_);
  }
}

void ConnectorsManager::CacheReportingConnectorPolicy(
    ReportingConnector connector) {
  reporting_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue* policy_value =
      pref_change_registrar_.prefs()->GetList(pref);
  if (policy_value && policy_value->is_list()) {
    for (const base::Value& service_settings : policy_value->GetList())
      reporting_connector_settings_[connector].emplace_back(
          service_settings, *service_provider_config_);
  }
}

void ConnectorsManager::CacheFileSystemConnectorPolicy(
    FileSystemConnector connector) {
  file_system_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue* policy_value =
      pref_change_registrar_.prefs()->GetList(pref);
  if (policy_value && policy_value->is_list()) {
    for (const base::Value& service_settings : policy_value->GetList())
      file_system_connector_settings_[connector].emplace_back(
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

void ConnectorsManager::StartObservingPrefs(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  StartObservingPref(AnalysisConnector::FILE_ATTACHED);
  StartObservingPref(AnalysisConnector::FILE_DOWNLOADED);
  StartObservingPref(AnalysisConnector::BULK_DATA_ENTRY);
  StartObservingPref(ReportingConnector::SECURITY_EVENT);
  StartObservingPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
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

void ConnectorsManager::StartObservingPref(FileSystemConnector connector) {
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref,
        base::BindRepeating(&ConnectorsManager::CacheFileSystemConnectorPolicy,
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

const ConnectorsManager::FileSystemConnectorsSettings&
ConnectorsManager::GetFileSystemConnectorsSettingsForTesting() const {
  return file_system_connector_settings_;
}

}  // namespace enterprise_connectors

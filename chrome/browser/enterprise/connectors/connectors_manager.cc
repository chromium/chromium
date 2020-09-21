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

const base::Feature kEnterpriseConnectorsEnabled{
    "EnterpriseConnectorsEnabled", base::FEATURE_ENABLED_BY_DEFAULT};

const char kServiceProviderConfig[] = R"({
  "version": "1",
  "service_providers" : [
    {
      "name": "google",
      "display_name": "Google Cloud",
      "version": {
        "1": {
          "analysis": {
            "url": "https://safebrowsing.google.com/safebrowsing/uploads/scan",
            "supported_tags": [
              {
                "name": "malware",
                "display_name": "Threat protection",
                "mime_types": [
                  "application/vnd.microsoft.portable-executable",
                  "application/vnd.rar",
                  "application/x-msdos-program",
                  "application/zip"
                ],
                "max_file_size": 52428800
              },
              {
                "name": "dlp",
                "display_name": "Sensitive data protection",
                "mime_types": [
                  "application/gzip",
                  "application/msword",
                  "application/pdf",
                  "application/postscript",
                  "application/rtf",
                  "application/vnd.google-apps.document.internal",
                  "application/vnd.google-apps.spreadsheet.internal",
                  "application/vnd.ms-cab-compressed",
                  "application/vnd.ms-excel",
                  "application/vnd.ms-powerpoint",
                  "application/vnd.ms-xpsdocument",
                  "application/vnd.oasis.opendocument.text",
                  "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                  "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
                  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                  "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
                  "application/vnd.ms-excel.sheet.macroenabled.12",
                  "application/vnd.ms-excel.template.macroenabled.12",
                  "application/vnd.ms-word.document.macroenabled.12",
                  "application/vnd.ms-word.template.macroenabled.12",
                  "application/vnd.rar",
                  "application/vnd.wordperfect",
                  "application/x-7z-compressed",
                  "application/x-bzip",
                  "application/x-bzip2",
                  "application/x-tar",
                  "application/zip",
                  "text/csv",
                  "text/plain"
                ],
                "max_file_size": 52428800
              }
            ]
          },
          "reporting": {
            "url": "https://chromereporting-pa.googleapis.com/v1/events"
          }
        }
      }
    }
  ]
})";

namespace {

base::ListValue AllPatterns() {
  auto v = std::vector<base::Value>();
  v.emplace_back("*");
  return base::ListValue(std::move(v));
}

bool MatchURLAgainstPatterns(const GURL& url,
                             const base::ListValue* patterns_to_scan,
                             const base::ListValue* patterns_to_exempt) {
  url_matcher::URLMatcher matcher;
  url_matcher::URLMatcherConditionSet::ID id(0);

  policy::url_util::AddFilters(&matcher, true, &id, patterns_to_scan);

  url_matcher::URLMatcherConditionSet::ID last_allowed_id = id;

  policy::url_util::AddFilters(&matcher, false, &id, patterns_to_exempt);

  auto matches = matcher.MatchURL(url);
  bool has_scan_match = false;
  for (const auto& match_id : matches) {
    if (match_id <= last_allowed_id)
      has_scan_match = true;
    else
      return false;
  }
  return has_scan_match;
}

}  // namespace

// ConnectorsManager implementation---------------------------------------------
ConnectorsManager::ConnectorsManager() {
  StartObservingPrefs();
}

ConnectorsManager::~ConnectorsManager() = default;

// static
ConnectorsManager* ConnectorsManager::GetInstance() {
  static base::NoDestructor<ConnectorsManager> manager;
  return manager.get();
}

bool ConnectorsManager::IsConnectorEnabled(AnalysisConnector connector) const {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  if (analysis_connector_settings_.count(connector) == 1)
    return true;

  const char* pref = ConnectorPref(connector);
  return pref && g_browser_process->local_state()->HasPrefPath(pref);
}

bool ConnectorsManager::IsConnectorEnabled(ReportingConnector connector) const {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  if (reporting_connector_settings_.count(connector) == 1)
    return true;

  const char* pref = ConnectorPref(connector);
  return pref && g_browser_process->local_state()->HasPrefPath(pref);
}

base::Optional<ReportingSettings> ConnectorsManager::GetReportingSettings(
    ReportingConnector connector) {
  // Prioritize new Connector policies over legacy ones.
  if (IsConnectorEnabled(connector))
    return GetReportingSettingsFromConnectorPolicy(connector);

  return GetReportingSettingsFromLegacyPolicies(connector);
}

base::Optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  // Prioritize new Connector policies over legacy ones.
  if (IsConnectorEnabled(connector))
    return GetAnalysisSettingsFromConnectorPolicy(url, connector);

  return GetAnalysisSettingsFromLegacyPolicies(url, connector);
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

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue* policy_value =
      g_browser_process->local_state()->GetList(pref);
  if (policy_value && policy_value->is_list()) {
    for (const base::Value& service_settings : policy_value->GetList())
      analysis_connector_settings_[connector].emplace_back(
          service_settings, service_provider_config_);
  }
}

void ConnectorsManager::CacheReportingConnectorPolicy(
    ReportingConnector connector) {
  reporting_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = ConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue* policy_value =
      g_browser_process->local_state()->GetList(pref);
  if (policy_value && policy_value->is_list()) {
    for (const base::Value& service_settings : policy_value->GetList())
      reporting_connector_settings_[connector].emplace_back(
          service_settings, service_provider_config_);
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
    return false;
  } else {
    bool upload = connector != AnalysisConnector::FILE_DOWNLOADED;
    return LegacyBlockUntilVerdict(upload) == BlockUntilVerdict::BLOCK;
  }
}

base::Optional<AnalysisSettings>
ConnectorsManager::GetAnalysisSettingsFromLegacyPolicies(
    const GURL& url,
    AnalysisConnector connector) const {
  // Legacy policies map to upload/download instead of individual hooks.
  bool upload = connector != AnalysisConnector::FILE_DOWNLOADED;

  std::set<std::string> tags = MatchURLAgainstLegacyPolicies(url, upload);

  // No tags implies the legacy patterns policies didn't match the URL, so no
  // settings are returned.
  if (tags.empty())
    return base::nullopt;

  auto settings = AnalysisSettings();
  settings.block_until_verdict = LegacyBlockUntilVerdict(upload);
  settings.block_password_protected_files =
      LegacyBlockPasswordProtectedFiles(upload);
  settings.block_large_files = LegacyBlockLargeFiles(upload);
  settings.block_unsupported_file_types =
      LegacyBlockUnsupportedFileTypes(upload);
  settings.tags = std::move(tags);

  return settings;
}

BlockUntilVerdict ConnectorsManager::LegacyBlockUntilVerdict(
    bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kDelayDeliveryUntilVerdict);
  if (pref == safe_browsing::DELAY_NONE)
    return BlockUntilVerdict::NO_BLOCK;
  if (pref == safe_browsing::DELAY_UPLOADS_AND_DOWNLOADS)
    return BlockUntilVerdict::BLOCK;
  return ((upload && pref == safe_browsing::DELAY_UPLOADS) ||
          (!upload && pref == safe_browsing::DELAY_DOWNLOADS))
             ? BlockUntilVerdict::BLOCK
             : BlockUntilVerdict::NO_BLOCK;
}

bool ConnectorsManager::LegacyBlockPasswordProtectedFiles(bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kAllowPasswordProtectedFiles);
  if (pref == safe_browsing::ALLOW_NONE)
    return true;
  if (pref == safe_browsing::ALLOW_UPLOADS_AND_DOWNLOADS)
    return false;
  return upload ? pref != safe_browsing::ALLOW_UPLOADS
                : pref != safe_browsing::ALLOW_DOWNLOADS;
}

bool ConnectorsManager::LegacyBlockLargeFiles(bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kBlockLargeFileTransfer);
  if (pref == safe_browsing::BLOCK_NONE)
    return false;
  if (pref == safe_browsing::BLOCK_LARGE_UPLOADS_AND_DOWNLOADS)
    return true;
  return upload ? pref == safe_browsing::BLOCK_LARGE_UPLOADS
                : pref == safe_browsing::BLOCK_LARGE_DOWNLOADS;
}

bool ConnectorsManager::LegacyBlockUnsupportedFileTypes(bool upload) const {
  int pref = g_browser_process->local_state()->GetInteger(
      prefs::kBlockUnsupportedFiletypes);
  if (pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_NONE)
    return false;
  if (pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS)
    return true;
  return upload ? pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS
                : pref == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS;
}

bool ConnectorsManager::MatchURLAgainstLegacyDlpPolicies(const GURL& url,
                                                         bool upload) const {
  const base::ListValue all_patterns = AllPatterns();
  const base::ListValue no_patterns = base::ListValue();

  const base::ListValue* patterns_to_scan;
  const base::ListValue* patterns_to_exempt;
  if (upload) {
    patterns_to_scan = &all_patterns;
    patterns_to_exempt = g_browser_process->local_state()->GetList(
        prefs::kURLsToNotCheckComplianceOfUploadedContent);
  } else {
    patterns_to_scan = g_browser_process->local_state()->GetList(
        prefs::kURLsToCheckComplianceOfDownloadedContent);
    patterns_to_exempt = &no_patterns;
  }

  return MatchURLAgainstPatterns(url, patterns_to_scan, patterns_to_exempt);
}

bool ConnectorsManager::MatchURLAgainstLegacyMalwarePolicies(
    const GURL& url,
    bool upload) const {
  const base::ListValue all_patterns = AllPatterns();
  const base::ListValue no_patterns = base::ListValue();

  const base::ListValue* patterns_to_scan;
  const base::ListValue* patterns_to_exempt;
  if (upload) {
    patterns_to_scan = g_browser_process->local_state()->GetList(
        prefs::kURLsToCheckForMalwareOfUploadedContent);
    patterns_to_exempt = &no_patterns;
  } else {
    patterns_to_scan = &all_patterns;
    patterns_to_exempt = g_browser_process->local_state()->GetList(
        prefs::kURLsToNotCheckForMalwareOfDownloadedContent);
  }

  return MatchURLAgainstPatterns(url, patterns_to_scan, patterns_to_exempt);
}

std::set<std::string> ConnectorsManager::MatchURLAgainstLegacyPolicies(
    const GURL& url,
    bool upload) const {
  std::set<std::string> tags;

  if (MatchURLAgainstLegacyDlpPolicies(url, upload))
    tags.emplace("dlp");

  if (MatchURLAgainstLegacyMalwarePolicies(url, upload))
    tags.emplace("malware");

  return tags;
}

base::Optional<ReportingSettings>
ConnectorsManager::GetReportingSettingsFromConnectorPolicy(
    ReportingConnector connector) {
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

base::Optional<ReportingSettings>
ConnectorsManager::GetReportingSettingsFromLegacyPolicies(
    ReportingConnector connector) const {
  if (!g_browser_process || !g_browser_process->local_state() ||
      !g_browser_process->local_state()->GetBoolean(
          prefs::kUnsafeEventsReportingEnabled)) {
    return base::nullopt;
  }

  return ReportingSettings(
      GURL("https://chromereporting-pa.googleapis.com/v1/events"));
}

void ConnectorsManager::StartObservingPrefs() {
  pref_change_registrar_.Init(g_browser_process->local_state());
  if (base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled)) {
    StartObservingPref(AnalysisConnector::FILE_ATTACHED);
    StartObservingPref(AnalysisConnector::FILE_DOWNLOADED);
    StartObservingPref(AnalysisConnector::BULK_DATA_ENTRY);
    StartObservingPref(ReportingConnector::SECURITY_EVENT);
  }
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

void ConnectorsManager::SetUpForTesting() {
  StartObservingPrefs();
}

void ConnectorsManager::TearDownForTesting() {
  pref_change_registrar_.RemoveAll();
  ClearCacheForTesting();
}

void ConnectorsManager::ClearCacheForTesting() {
  analysis_connector_settings_.clear();
  reporting_connector_settings_.clear();
}

}  // namespace enterprise_connectors

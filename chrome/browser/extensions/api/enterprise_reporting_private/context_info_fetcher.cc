// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/context_info_fetcher.h"

#include <memory>

namespace extensions {
namespace enterprise_reporting {

ContextInfoFetcher::ContextInfoFetcher(
    enterprise_connectors::ConnectorsService* connectors_service)
    : connectors_service_(connectors_service) {
  DCHECK(connectors_service_);
}

ContextInfoFetcher::~ContextInfoFetcher() = default;

// static
std::unique_ptr<ContextInfoFetcher> ContextInfoFetcher::CreateInstance(
    enterprise_connectors::ConnectorsService* connectors_service) {
  // TODO(domfc): Add platform overrides of the class once they are needed for
  // an attribute.
  return std::make_unique<ContextInfoFetcher>(connectors_service);
}

api::enterprise_reporting_private::ContextInfo ContextInfoFetcher::Fetch() {
  api::enterprise_reporting_private::ContextInfo info;

  info.browser_affiliation_ids = GetBrowserAffiliationIDs();
  info.profile_affiliation_ids = GetProfileAffiliationIDs();
  info.on_file_attached_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::FILE_ATTACHED);
  info.on_file_downloaded_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::FILE_DOWNLOADED);
  info.on_bulk_data_entry_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::BULK_DATA_ENTRY);
  info.realtime_url_check_mode = GetRealtimeUrlCheckMode();
  info.on_security_event_providers = GetOnSecurityEventProviders();
  info.browser_version = GetBrowserVersion();

  return info;
}

std::vector<std::string> ContextInfoFetcher::GetBrowserAffiliationIDs() {
  // TODO(crbug.com/1169200): Add code to get the affiliation IDs.
  return {};
}

std::vector<std::string> ContextInfoFetcher::GetProfileAffiliationIDs() {
  // TODO(crbug.com/1169212): Add code to get the affiliation IDs.
  return {};
}

std::vector<std::string> ContextInfoFetcher::GetAnalysisConnectorProviders(
    enterprise_connectors::AnalysisConnector connector) {
  // TODO(crbug.com/1169213): Add code here and in ConnectorsService to get each
  // Analysis Connector's providers.
  return {};
}

api::enterprise_reporting_private::RealtimeUrlCheckMode
ContextInfoFetcher::GetRealtimeUrlCheckMode() {
  // TODO(crbug.com/1169214): Add code here and in ConnectorsService to obtain
  // the state of real time URL checks.
  return api::enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED;
}

std::vector<std::string> ContextInfoFetcher::GetOnSecurityEventProviders() {
  // TODO(crbug.com/1169219): Add code here and in ConnectorsService to obtain
  // the providers of this policy.
  return {};
}

std::string ContextInfoFetcher::GetBrowserVersion() {
  // TODO(crbug.com/1169222): Add code to obtain the browser version.
  return "";
}

}  // namespace enterprise_reporting
}  // namespace extensions

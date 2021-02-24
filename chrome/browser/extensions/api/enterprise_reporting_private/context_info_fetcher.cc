// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/context_info_fetcher.h"

#include <memory>
#include "chrome/common/extensions/api/enterprise_reporting_private.h"

#include "base/callback_forward.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/version_info/version_info.h"
#include "device_management_backend.pb.h"

namespace extensions {
namespace enterprise_reporting {

ContextInfoFetcher::ContextInfoFetcher(
    content::BrowserContext* browser_context,
    enterprise_connectors::ConnectorsService* connectors_service)
    : browser_context_(browser_context),
      connectors_service_(connectors_service) {
  DCHECK(connectors_service_);
}

ContextInfoFetcher::~ContextInfoFetcher() = default;

// static
std::unique_ptr<ContextInfoFetcher> ContextInfoFetcher::CreateInstance(
    content::BrowserContext* browser_context,
    enterprise_connectors::ConnectorsService* connectors_service) {
  // TODO(domfc): Add platform overrides of the class once they are needed for
  // an attribute.
  return std::make_unique<ContextInfoFetcher>(browser_context,
                                              connectors_service);
}

void ContextInfoFetcher::Fetch(ContextInfoCallback callback) {
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
  info.browser_version = version_info::GetVersionNumber();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(info)));
}

std::vector<std::string> ContextInfoFetcher::GetBrowserAffiliationIDs() {
  const enterprise_management::PolicyData* browser_policy_data =
      chrome::enterprise_util::GetBrowserPolicyData();

  if (!browser_policy_data ||
      browser_policy_data->device_affiliation_ids().empty()) {
    return {};
  }

  const auto& affiliation_ids = browser_policy_data->device_affiliation_ids();
  return std::vector<std::string>(affiliation_ids.begin(),
                                  affiliation_ids.end());
}

std::vector<std::string> ContextInfoFetcher::GetProfileAffiliationIDs() {
  const enterprise_management::PolicyData* profile_policy_data =
      chrome::enterprise_util::GetProfilePolicyData(
          Profile::FromBrowserContext(browser_context_));

  if (!profile_policy_data ||
      profile_policy_data->user_affiliation_ids().empty()) {
    return {};
  }

  const auto& affiliation_ids = profile_policy_data->user_affiliation_ids();
  return std::vector<std::string>(affiliation_ids.begin(),
                                  affiliation_ids.end());
}

std::vector<std::string> ContextInfoFetcher::GetAnalysisConnectorProviders(
    enterprise_connectors::AnalysisConnector connector) {
  return connectors_service_->GetAnalysisServiceProviderNames(connector);
}

api::enterprise_reporting_private::RealtimeUrlCheckMode
ContextInfoFetcher::GetRealtimeUrlCheckMode() {
  switch (connectors_service_->GetAppliedRealTimeUrlCheck()) {
    case safe_browsing::REAL_TIME_CHECK_DISABLED:
      return api::enterprise_reporting_private::
          REALTIME_URL_CHECK_MODE_DISABLED;
    case safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED:
      return api::enterprise_reporting_private::
          REALTIME_URL_CHECK_MODE_ENABLED_MAIN_FRAME;
  }
}

std::vector<std::string> ContextInfoFetcher::GetOnSecurityEventProviders() {
  return connectors_service_->GetReportingServiceProviderNames(
      enterprise_connectors::ReportingConnector::SECURITY_EVENT);
}

}  // namespace enterprise_reporting
}  // namespace extensions

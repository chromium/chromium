// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/chrome_browser_main_extra_parts_enterprise.h"

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"

namespace enterprise_util {

namespace {

// If a local agent has been configured for content analysis, this function
// connects to the agent immediately.  Agents expect chrome to connect to them
// at some point during startup to determine if chrome is configuredcorrectly.
void MaybePrimeLocalContentAnalysisAgentConnection(Profile* profile) {
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  constexpr enterprise_connectors::AnalysisConnector kConnectors[] = {
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
      enterprise_connectors::AnalysisConnector::PRINT,
  };
  for (auto connector : kConnectors) {
    // While the connector service supports more than one config per connector,
    // today only one is supported.
    auto configs = connectors_service->GetAnalysisServiceConfigs(connector);
    if (configs.size() < 1 || !configs[0]->local_path)
      continue;

    // Prime the SDK manager with the correct client.
    enterprise_connectors::ContentAnalysisSdkManager::Get()->GetClient(
        {configs[0]->local_path, configs[0]->user_specific});
  }
}

}  // namespace

ChromeBrowserMainExtraPartsEnterprise::ChromeBrowserMainExtraPartsEnterprise() =
    default;

ChromeBrowserMainExtraPartsEnterprise::
    ~ChromeBrowserMainExtraPartsEnterprise() = default;

void ChromeBrowserMainExtraPartsEnterprise::PostProfileInit(
    Profile* profile,
    bool is_initial_profile) {
  MaybePrimeLocalContentAnalysisAgentConnection(profile);
}

}  // namespace enterprise_util

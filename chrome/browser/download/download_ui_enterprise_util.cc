// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_enterprise_util.h"

#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#endif

namespace download {

bool DoesDownloadConnectorBlock(Profile* profile, const GURL& url) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  auto* connector_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  if (!connector_service) {
    return false;
  }

  std::optional<enterprise_connectors::AnalysisSettings> settings =
      connector_service->GetAnalysisSettings(
          url, enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
  if (!settings) {
    return false;
  }

  return settings->block_until_verdict ==
         enterprise_connectors::BlockUntilVerdict::kBlock;
#else
  return false;
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

}  // namespace download

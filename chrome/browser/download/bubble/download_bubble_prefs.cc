// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_prefs.h"

#include "base/feature_list.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace download {

bool IsDownloadBubbleEnabled(Profile* profile) {
  if (!base::FeatureList::IsEnabled(safe_browsing::kDownloadBubble)) {
    return false;
  }

  // TODO(crbug.com/1307021): Enable download bubble for enhanced protection
  // users, advanced protection users and enterprise connector users once it
  // supports deep scanning.
  if (safe_browsing::IsEnhancedProtectionEnabled(*profile->GetPrefs())) {
    return false;
  }

  auto* advanced_protection_manager =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile);
  if (advanced_protection_manager &&
      advanced_protection_manager->IsUnderAdvancedProtection()) {
    return false;
  }

  auto* connector_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  if (connector_service &&
      connector_service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED)) {
    return false;
  }

  // TODO(crbug.com/1307021): Create an enterprise policy DownloadBubbleEnabled
  // and check here.
  return true;
}

}  // namespace download

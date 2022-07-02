// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_prefs.h"

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace download {

bool IsDownloadBubbleEnabled(Profile* profile) {
// Download bubble won't replace the old download notification in
// Ash. See https://crbug.com/1323505.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  if (!base::FeatureList::IsEnabled(safe_browsing::kDownloadBubble)) {
    return false;
  }

  PrefService* prefs = profile->GetPrefs();

  // If the download bubble policy is managed by enterprise admins and it is
  // set to false, disable download bubble.
  if (prefs->IsManagedPreference(prefs::kDownloadBubbleEnabled) &&
      !prefs->GetBoolean(prefs::kDownloadBubbleEnabled)) {
    return false;
  }

  return true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool IsDownloadBubbleV2Enabled(Profile* profile) {
  return IsDownloadBubbleEnabled(profile) &&
         base::FeatureList::IsEnabled(safe_browsing::kDownloadBubbleV2);
}

bool IsDownloadConnectorEnabled(Profile* profile) {
  auto* connector_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  return connector_service &&
         connector_service->IsConnectorEnabled(
             enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
}

}  // namespace download

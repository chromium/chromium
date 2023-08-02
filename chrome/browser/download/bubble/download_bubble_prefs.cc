// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_prefs.h"

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

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

bool ShouldShowDownloadBubble(Profile* profile) {
  // If the download UI is disabled by at least one extension, do not show the
  // bubble and the toolbar icon.
  return DownloadCoreServiceFactory::GetForBrowserContext(
             profile->GetOriginalProfile())
      ->IsDownloadUiEnabled();
}

bool DoesDownloadConnectorBlock(Profile* profile, const GURL& url) {
  auto* connector_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  if (!connector_service) {
    return false;
  }

  absl::optional<enterprise_connectors::AnalysisSettings> settings =
      connector_service->GetAnalysisSettings(
          url, enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
  if (!settings) {
    return false;
  }

  return settings->block_until_verdict ==
         enterprise_connectors::BlockUntilVerdict::kBlock;
}

bool ShouldSuppressDownloadBubbleIph(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kDownloadBubbleIphSuppression);
}

bool IsDownloadBubblePartialViewControlledByPref() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Disable "show downloads when they're done" by default when SysUI downloads
  // integration V2 is enabled in Ash to prevent competition for the user's
  // attention on download completion.
  return !chromeos::BrowserParamsProxy::Get()
              ->IsSysUiDownloadsIntegrationV2Enabled();
#else
  return true;
#endif
}

bool IsDownloadBubblePartialViewEnabled(Profile* profile) {
  if (!IsDownloadBubblePartialViewControlledByPref()) {
    return false;
  }
  return profile->GetPrefs()->GetBoolean(
      prefs::kDownloadBubblePartialViewEnabled);
}

void SetDownloadBubblePartialViewEnabled(Profile* profile, bool enabled) {
  profile->GetPrefs()->SetBoolean(prefs::kDownloadBubblePartialViewEnabled,
                                  enabled);
}

bool IsDownloadBubblePartialViewEnabledDefaultPrefValue(Profile* profile) {
  if (!IsDownloadBubblePartialViewControlledByPref()) {
    return false;
  }
  return profile->GetPrefs()
      ->FindPreference(prefs::kDownloadBubblePartialViewEnabled)
      ->IsDefaultValue();
}

int DownloadBubblePartialViewImpressions(Profile* profile) {
  return profile->GetPrefs()->GetInteger(
      prefs::kDownloadBubblePartialViewImpressions);
}

void SetDownloadBubblePartialViewImpressions(Profile* profile, int count) {
  profile->GetPrefs()->SetInteger(prefs::kDownloadBubblePartialViewImpressions,
                                  count);
}

}  // namespace download

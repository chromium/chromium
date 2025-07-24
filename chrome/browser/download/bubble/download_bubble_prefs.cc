// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_prefs.h"

#include "base/feature_list.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"

namespace download {

bool IsDownloadBubbleEnabled() {
// Download bubble won't replace the old download notification in
// Ash. See https://crbug.com/1323505.
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool ShouldShowDownloadBubble(Profile* profile) {
  // There is no need to display download bubble in headless mode. This prevents
  // http://crbug.com/379994807 but also makes sense on other platforms.
  if (headless::IsHeadlessMode()) {
    return false;
  }
  // If the download UI is disabled by at least one extension, do not show the
  // bubble and the toolbar icon.
  return DownloadCoreServiceFactory::GetForBrowserContext(
             profile->GetOriginalProfile())
      ->IsDownloadUiEnabled();
}

bool IsDownloadBubblePartialViewControlledByPref() {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
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

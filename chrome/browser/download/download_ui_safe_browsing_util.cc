// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_safe_browsing_util.h"

#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#endif

bool WasSafeBrowsingVerdictObtained(const download::DownloadItem* item) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return item &&
         safe_browsing::DownloadProtectionService::HasDownloadProtectionVerdict(
             item);
#else
  return false;
#endif
}

bool ShouldShowWarningForNoSafeBrowsing(Profile* profile) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return safe_browsing::GetSafeBrowsingState(*profile->GetPrefs()) ==
         safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING;
#else
  return true;
#endif
}

bool CanUserTurnOnSafeBrowsing(Profile* profile) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  return !safe_browsing::IsSafeBrowsingPolicyManaged(*profile->GetPrefs());
#else
  return false;
#endif
}

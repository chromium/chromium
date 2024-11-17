// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_safe_browsing_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

namespace {

#if BUILDFLAG(FULL_SAFE_BROWSING)
using safe_browsing::ClientDownloadResponse;
using safe_browsing::ClientSafeBrowsingReportRequest;
#endif

std::string GetDangerPromptHistogramName(const std::string& suffix,
                                         const download::DownloadItem& item) {
  const char kPrefix[] = "Download.DownloadDangerPrompt";
  download::DownloadDangerType danger_type = item.GetDangerType();
  return base::StringPrintf("%s.%s.%s", kPrefix,
                            download::GetDownloadDangerTypeString(danger_type),
                            // "Proceed" or "Shown".
                            suffix.c_str());
}

}  // namespace

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

void RecordDownloadDangerPromptHistogram(
    const std::string& proceed_or_shown_suffix,
    const download::DownloadItem& item) {
  int64_t file_type_uma_value =
      safe_browsing::FileTypePolicies::GetInstance()->UmaValueForFile(
          item.GetTargetFilePath());
  base::UmaHistogramSparse(
      GetDangerPromptHistogramName(proceed_or_shown_suffix, item),
      file_type_uma_value);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void SendSafeBrowsingDownloadReport(
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    download::DownloadItem* item) {
  if (safe_browsing::SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service()) {
    sb_service->SendDownloadReport(item, report_type, did_proceed,
                                   /*show_download_in_folder=*/std::nullopt);
  }
}
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

bool ShouldShowDeepScanPromptNotice(Profile* profile,
                                    download::DownloadDangerType danger_type) {
  if (danger_type != download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
    return false;
  }

  if (!safe_browsing::IsEnhancedProtectionEnabled(*profile->GetPrefs())) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          safe_browsing::kDeepScanningPromptRemoval)) {
    return false;
  }

  if (profile->GetPrefs()->GetBoolean(
          prefs::kSafeBrowsingAutomaticDeepScanPerformed)) {
    return false;
  }

  return true;
}

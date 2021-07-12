// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"

using safe_browsing::ClientDownloadResponse;
using safe_browsing::ClientSafeBrowsingReportRequest;

namespace {

const char kDownloadDangerPromptPrefix[] = "Download.DownloadDangerPrompt";

// Converts DownloadDangerType into their corresponding string.
const char* GetDangerTypeString(
    const download::DownloadDangerType& danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DangerousFile";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DangerousURL";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DangerousContent";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DangerousHost";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UncommonContent";
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "PotentiallyUnwanted";
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return "AsyncScanning";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return "BlockedPasswordProtected";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return "BlockedTooLarge";
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return "SensitiveContentWarning";
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return "SensitiveContentBlock";
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return "DeepScannedSafe";
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return "DeepScannedOpenedDangerous";
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
      return "PromptForScanning";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
      return "BlockedUnsupportedFiletype";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return "DangerousAccountCompromise";
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

void DownloadDangerPrompt::SendSafeBrowsingDownloadReport(
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    const download::DownloadItem& download) {
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(&download));
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(report_type);
  ClientDownloadResponse::Verdict download_verdict =
      safe_browsing::DownloadDangerTypeToDownloadResponseVerdict(
          download.GetDangerType());
  if (download_verdict == ClientDownloadResponse::SAFE) {
    // Don't send report if the verdict is SAFE.
    return;
  }
  report->set_download_verdict(download_verdict);
  report->set_url(download.GetURL().spec());
  report->set_did_proceed(did_proceed);
  std::string token =
      safe_browsing::DownloadProtectionService::GetDownloadPingToken(&download);
  if (!token.empty())
    report->set_token(token);
  std::string serialized_report;
  if (report->SerializeToString(&serialized_report)) {
    sb_service->SendSerializedDownloadReport(profile, serialized_report);

    // The following is to log this ClientSafeBrowsingReportRequest on any open
    // chrome://safe-browsing pages.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &safe_browsing::WebUIInfoSingleton::AddToCSBRRsSent,
            base::Unretained(safe_browsing::WebUIInfoSingleton::GetInstance()),
            std::move(report)));
  } else {
    DLOG(ERROR) << "Unable to serialize the threat report.";
  }
}

void DownloadDangerPrompt::RecordDownloadDangerPrompt(
    bool did_proceed,
    const download::DownloadItem& download) {
  int64_t file_type_uma_value =
      safe_browsing::FileTypePolicies::GetInstance()->UmaValueForFile(
          download.GetTargetFilePath());
  download::DownloadDangerType danger_type = download.GetDangerType();

  base::UmaHistogramSparse(
      base::StringPrintf("%s.%s.Shown", kDownloadDangerPromptPrefix,
                         GetDangerTypeString(danger_type)),
      file_type_uma_value);
  if (did_proceed) {
    base::UmaHistogramSparse(
        base::StringPrintf("%s.%s.Proceed", kDownloadDangerPromptPrefix,
                           GetDangerTypeString(danger_type)),
        file_type_uma_value);
  }
}

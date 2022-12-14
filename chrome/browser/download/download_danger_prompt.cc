// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_warning_data.h"
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
}  // namespace

void DownloadDangerPrompt::SendSafeBrowsingDownloadReport(
    ClientSafeBrowsingReportRequest::ReportType report_type,
    bool did_proceed,
    download::DownloadItem* download) {
  ClientDownloadResponse::Verdict download_verdict =
      safe_browsing::DownloadProtectionService::GetDownloadProtectionVerdict(
          download);
  if (download_verdict == ClientDownloadResponse::SAFE) {
    // Don't send report if the verdict is SAFE.
    return;
  }
  g_browser_process->safe_browsing_service()->SendDownloadReport(
      download, report_type, did_proceed,
      /*show_download_in_folder=*/absl::nullopt);
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
                         download::GetDownloadDangerTypeString(danger_type)),
      file_type_uma_value);
  if (did_proceed) {
    base::UmaHistogramSparse(
        base::StringPrintf("%s.%s.Proceed", kDownloadDangerPromptPrefix,
                           download::GetDownloadDangerTypeString(danger_type)),
        file_type_uma_value);
  }
}

void DownloadDangerPrompt::RecordDownloadWarningEvent(
    Action action,
    download::DownloadItem* download) {
  DownloadItemWarningData::WarningAction warning_action;
  switch (action) {
    case Action::ACCEPT:
      warning_action = DownloadItemWarningData::WarningAction::PROCEED;
      break;
    case Action::CANCEL:
      warning_action = DownloadItemWarningData::WarningAction::CANCEL;
      break;
    case Action::DISMISS:
      warning_action = DownloadItemWarningData::WarningAction::CLOSE;
      break;
  }
  DownloadItemWarningData::AddWarningActionEvent(
      download, DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT,
      warning_action);
}

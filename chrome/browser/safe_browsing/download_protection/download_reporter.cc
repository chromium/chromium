// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_reporter.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

namespace {

bool DangerTypeIsDangerous(download::DownloadDangerType danger_type) {
  return (danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED);
}

std::string DangerTypeToThreatType(download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DANGEROUS_FILE_TYPE";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DANGEROUS_URL";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DANGEROUS";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UNCOMMON";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    default:
      // Expects to only be called with the dangerous threat types listed above.
      NOTREACHED() << "Unexpected danger type: " << danger_type;
      return "UNKNOWN";
  }
}

void MaybeReportDangerousDownloadWarning(download::DownloadItem* download) {
  // If |download| has a deep scanning malware verdict, then it means the
  // dangerous file has already been reported.
  auto* scan_result = static_cast<enterprise_connectors::ScanResult*>(
      download->GetUserData(enterprise_connectors::ScanResult::kKey));
  if (scan_result &&
      enterprise_connectors::ContainsMalwareVerdict(scan_result->response)) {
    return;
  }

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile) {
    std::string raw_digest_sha256 = download->GetHash();
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnDangerousDownloadEvent(
            download->GetURL(), download->GetTargetFilePath().AsUTF8Unsafe(),
            base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
            DangerTypeToThreatType(download->GetDangerType()),
            download->GetMimeType(), download->GetTotalBytes(),
            EventResult::WARNED);
  }
}

void ReportDangerousDownloadWarningBypassed(
    download::DownloadItem* download,
    download::DownloadDangerType original_danger_type) {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile) {
    std::string raw_digest_sha256 = download->GetHash();
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnDangerousDownloadWarningBypassed(
            download->GetURL(), download->GetTargetFilePath().AsUTF8Unsafe(),
            base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
            DangerTypeToThreatType(original_danger_type),
            download->GetMimeType(), download->GetTotalBytes());
  }
}

void ReportAnalysisConnectorWarningBypassed(download::DownloadItem* download) {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile) {
    std::string raw_digest_sha256 = download->GetHash();
    enterprise_connectors::ScanResult* stored_result =
        static_cast<enterprise_connectors::ScanResult*>(
            download->GetUserData(enterprise_connectors::ScanResult::kKey));

    ReportAnalysisConnectorWarningBypass(
        profile, download->GetURL(),
        download->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
        download->GetMimeType(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
        DeepScanAccessPoint::DOWNLOAD, download->GetTotalBytes(),
        stored_result ? stored_result->response
                      : enterprise_connectors::ContentAnalysisResponse());
  }
}

}  // namespace

DownloadReporter::DownloadReporter() {
  // Profile manager can be null in unit tests.
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);
}

DownloadReporter::~DownloadReporter() {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
}

void DownloadReporter::OnProfileAdded(Profile* profile) {
  observed_profiles_.Add(profile);
  observed_coordinators_.Add(SimpleDownloadManagerCoordinatorFactory::GetForKey(
      profile->GetProfileKey()));
}

void DownloadReporter::OnOffTheRecordProfileCreated(Profile* off_the_record) {
  OnProfileAdded(off_the_record);
}

void DownloadReporter::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.Remove(profile);
}

void DownloadReporter::OnManagerGoingDown(
    download::SimpleDownloadManagerCoordinator* coordinator) {
  observed_coordinators_.Remove(coordinator);
}

void DownloadReporter::OnDownloadCreated(download::DownloadItem* download) {
  danger_types_[download] = download->GetDangerType();
  observed_downloads_.Add(download);
}

void DownloadReporter::OnDownloadDestroyed(download::DownloadItem* download) {
  observed_downloads_.Remove(download);
  danger_types_.erase(download);
}

void DownloadReporter::OnDownloadUpdated(download::DownloadItem* download) {
  // If the update isn't a change in danger type, we can ignore it.
  if (danger_types_[download] == download->GetDangerType())
    return;

  download::DownloadDangerType old_danger_type = danger_types_[download];
  download::DownloadDangerType current_danger_type = download->GetDangerType();

  if (!DangerTypeIsDangerous(old_danger_type) &&
      DangerTypeIsDangerous(current_danger_type)) {
    MaybeReportDangerousDownloadWarning(download);
  }

  if (DangerTypeIsDangerous(old_danger_type) &&
      current_danger_type == download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED) {
    ReportDangerousDownloadWarningBypassed(download, old_danger_type);
  }

  if (old_danger_type ==
          download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING &&
      current_danger_type == download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED) {
    ReportAnalysisConnectorWarningBypassed(download);
  }

  danger_types_[download] = current_danger_type;
}

}  // namespace safe_browsing

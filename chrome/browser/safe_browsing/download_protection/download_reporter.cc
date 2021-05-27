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
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/safe_browsing/core/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "url/url_constants.h"

namespace safe_browsing {

namespace {

bool DangerTypeIsDangerous(download::DownloadDangerType danger_type) {
  return (danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
          danger_type == download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED ||
          danger_type ==
              download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE);
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
    auto* router =
        extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
            profile);
    if (router) {
      router->OnDangerousDownloadEvent(
          download->GetURL(), download->GetTargetFilePath().AsUTF8Unsafe(),
          base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
          download->GetDangerType(), download->GetMimeType(), /*scan_id*/ "",
          download->GetTotalBytes(), EventResult::WARNED);
    }
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
    auto* router =
        extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
            profile);
    if (router) {
      enterprise_connectors::ScanResult* stored_result =
          static_cast<enterprise_connectors::ScanResult*>(
              download->GetUserData(enterprise_connectors::ScanResult::kKey));
      router->OnDangerousDownloadWarningBypassed(
          download->GetURL(), download->GetTargetFilePath().AsUTF8Unsafe(),
          base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
          original_danger_type, download->GetMimeType(),
          /*scan_id*/
          stored_result ? stored_result->response.request_token() : "",
          download->GetTotalBytes());
    }
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
  observed_profiles_.AddObservation(profile);
  observed_coordinators_.AddObservation(
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          profile->GetProfileKey()));
}

void DownloadReporter::OnOffTheRecordProfileCreated(Profile* off_the_record) {
  OnProfileAdded(off_the_record);
}

void DownloadReporter::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
}

void DownloadReporter::OnManagerGoingDown(
    download::SimpleDownloadManagerCoordinator* coordinator) {
  observed_coordinators_.RemoveObservation(coordinator);
}

void DownloadReporter::OnDownloadCreated(download::DownloadItem* download) {
  danger_types_[download] = download->GetDangerType();
  if (!observed_downloads_.IsObservingSource(download))
    observed_downloads_.AddObservation(download);
}

void DownloadReporter::OnDownloadDestroyed(download::DownloadItem* download) {
  observed_downloads_.RemoveObservation(download);
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
    AddBypassEventToPref(download);
    ReportDangerousDownloadWarningBypassed(download, old_danger_type);
    RecordDangerousDownloadWarningBypassed(
        old_danger_type, download->GetTargetFilePath(),
        download->GetURL().SchemeIs(url::kHttpsScheme),
        download->HasUserGesture());
  }

  if (old_danger_type ==
          download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING &&
      current_danger_type == download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED) {
    ReportAnalysisConnectorWarningBypassed(download);
  }

  danger_types_[download] = current_danger_type;
}

void DownloadReporter::AddBypassEventToPref(download::DownloadItem* download) {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile) {
    auto* metrics_collector =
        SafeBrowsingMetricsCollectorFactory::GetForProfile(profile);
    if (metrics_collector) {
      metrics_collector->AddSafeBrowsingEventToPref(
          SafeBrowsingMetricsCollector::EventType::DANGEROUS_DOWNLOAD_BYPASS);
    }
  }
}

}  // namespace safe_browsing

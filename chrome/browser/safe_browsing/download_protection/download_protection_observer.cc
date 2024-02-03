// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_observer.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/safe_browsing/content/browser/download/download_stats.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
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
  if (scan_result) {
    for (const auto& metadata : scan_result->file_metadata) {
      if (enterprise_connectors::ContainsMalwareVerdict(metadata.scan_response))
        return;
    }
  }

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return;

  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!router)
    return;

  router->OnDangerousDownloadEvent(
      download->GetURL(), download->GetTabUrl(),
      download->GetTargetFilePath().AsUTF8Unsafe(),
      base::HexEncode(download->GetHash()), download->GetDangerType(),
      download->GetMimeType(), /*scan_id*/ "", download->GetTotalBytes(),
      EventResult::WARNED);
}

void ReportDangerousDownloadWarningBypassed(
    download::DownloadItem* download,
    download::DownloadDangerType original_danger_type) {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return;

  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!router)
    return;

  enterprise_connectors::ScanResult* stored_result =
      static_cast<enterprise_connectors::ScanResult*>(
          download->GetUserData(enterprise_connectors::ScanResult::kKey));
  if (stored_result) {
    for (const auto& metadata : stored_result->file_metadata) {
      router->OnDangerousDownloadWarningBypassed(
          download->GetURL(), download->GetTabUrl(), metadata.filename,
          metadata.sha256, original_danger_type, metadata.mime_type,
          metadata.scan_response.request_token(), metadata.size);
    }
  } else {
    router->OnDangerousDownloadWarningBypassed(
        download->GetURL(), download->GetTabUrl(),
        download->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(download->GetHash()), original_danger_type,
        download->GetMimeType(),
        /*scan_id*/ "", download->GetTotalBytes());
  }
}

void ReportAnalysisConnectorWarningBypassed(download::DownloadItem* download) {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return;

  enterprise_connectors::ScanResult* stored_result =
      static_cast<enterprise_connectors::ScanResult*>(
          download->GetUserData(enterprise_connectors::ScanResult::kKey));

  if (stored_result) {
    for (const auto& metadata : stored_result->file_metadata) {
      ReportAnalysisConnectorWarningBypass(
          profile, download->GetURL(), download->GetTabUrl(), "", "",
          metadata.filename, metadata.sha256, metadata.mime_type,
          extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload, "",
          DeepScanAccessPoint::DOWNLOAD, metadata.size, metadata.scan_response,
          stored_result->user_justification);
    }
  } else {
    ReportAnalysisConnectorWarningBypass(
        profile, download->GetURL(), download->GetTabUrl(), "", "",
        download->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(download->GetHash()), download->GetMimeType(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload, "",
        DeepScanAccessPoint::DOWNLOAD, download->GetTotalBytes(),
        enterprise_connectors::ContentAnalysisResponse(),
        /*user_justification=*/std::nullopt);
  }
}

}  // namespace

DownloadProtectionObserver::DownloadProtectionObserver() {
  // Profile manager can be null in unit tests.
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);
}

DownloadProtectionObserver::~DownloadProtectionObserver() {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
}

void DownloadProtectionObserver::OnProfileAdded(Profile* profile) {
  observed_profiles_.AddObservation(profile);
  observed_coordinators_.AddObservation(
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          profile->GetProfileKey()));
}

void DownloadProtectionObserver::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  OnProfileAdded(off_the_record);
}

void DownloadProtectionObserver::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
}

void DownloadProtectionObserver::OnManagerGoingDown(
    download::SimpleDownloadManagerCoordinator* coordinator) {
  observed_coordinators_.RemoveObservation(coordinator);
}

void DownloadProtectionObserver::OnDownloadCreated(
    download::DownloadItem* download) {
  danger_types_[download] = download->GetDangerType();
  if (!observed_downloads_.IsObservingSource(download))
    observed_downloads_.AddObservation(download);
}

void DownloadProtectionObserver::OnDownloadDestroyed(
    download::DownloadItem* download) {
  observed_downloads_.RemoveObservation(download);
  danger_types_.erase(download);
}

void DownloadProtectionObserver::OnDownloadUpdated(
    download::DownloadItem* download) {
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
    ReportAndRecordDangerousDownloadWarningBypassed(download, old_danger_type);
  }

  if (old_danger_type ==
          download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING &&
      current_danger_type == download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED) {
    ReportAnalysisConnectorWarningBypassed(download);

    // This is called when a SavePackage warning is bypassed so that the final
    // renaming takes place.
    if (download->IsSavePackageDownload())
      enterprise_connectors::RunSavePackageScanningCallback(download, true);
  }

  danger_types_[download] = current_danger_type;
}

void DownloadProtectionObserver::OnDownloadRemoved(
    download::DownloadItem* download) {
  // This needs to run if a SavePackage is discarded after a scan so that it
  // can cleanup temporary files.
  if (download->IsSavePackageDownload())
    enterprise_connectors::RunSavePackageScanningCallback(download, false);
}

void DownloadProtectionObserver::ReportDelayedBypassEvent(
    download::DownloadItem* download,
    download::DownloadDangerType danger_type) {
  // Because the file was opened before the verdict was available, it's possible
  // that the danger type is "BLOCK" but the file was opened anyways.
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING ||
      danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK) {
    ReportAnalysisConnectorWarningBypassed(download);
  }

  if (DangerTypeIsDangerous(danger_type)) {
    ReportAndRecordDangerousDownloadWarningBypassed(download, danger_type);
  }
}

void DownloadProtectionObserver::AddBypassEventToPref(
    download::DownloadItem* download) {
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

void DownloadProtectionObserver::
    ReportAndRecordDangerousDownloadWarningBypassed(
        download::DownloadItem* download,
        download::DownloadDangerType danger_type) {
  AddBypassEventToPref(download);
  ReportDangerousDownloadWarningBypassed(download, danger_type);
  RecordDangerousDownloadWarningBypassed(
      danger_type, download->GetTargetFilePath(),
      download->GetURL().SchemeIs(url::kHttpsScheme),
      download->HasUserGesture());
}

}  // namespace safe_browsing

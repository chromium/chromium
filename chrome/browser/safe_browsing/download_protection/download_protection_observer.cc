// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_observer.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/safe_browsing/content/browser/download/download_stats.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#endif

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
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
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

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          browser_context);
  if (!router)
    return;

  ReferrerChain referrer_chain;
  if (base::FeatureList::IsEnabled(kEnhancedFieldsForSecOps)) {
    referrer_chain = GetOrIdentifyReferrerChainForEnterprise(*download);
  }

  router->OnDangerousDownloadEvent(
      download->GetURL(), download->GetTabUrl(),
      download->GetTargetFilePath().AsUTF8Unsafe(),
      base::HexEncode(download->GetHash()), download->GetDangerType(),
      download->GetMimeType(),
      enterprise_connectors::kFileDownloadDataTransferEventTrigger,
      /*scan_id=*/"", download->GetTotalBytes(), referrer_chain,
      enterprise_connectors::CollectFrameUrls(
          content::DownloadItemUtils::GetWebContents(download),
          enterprise_connectors::DeepScanAccessPoint::DOWNLOAD),
      enterprise_connectors::EventResult::WARNED);
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void ReportDangerousDownloadWarningBypassed(
    download::DownloadItem* download,
    download::DownloadDangerType original_danger_type) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);

  auto* router =
      enterprise_connectors::ReportingEventRouterFactory::GetForBrowserContext(
          browser_context);
  if (!router)
    return;

  ReferrerChain referrer_chain;
  if (base::FeatureList::IsEnabled(kEnhancedFieldsForSecOps)) {
    referrer_chain = GetOrIdentifyReferrerChainForEnterprise(*download);
  }

  google::protobuf::RepeatedPtrField<std::string> frame_url_chain =
      enterprise_connectors::CollectFrameUrls(
          content::DownloadItemUtils::GetWebContents(download),
          enterprise_connectors::DeepScanAccessPoint::DOWNLOAD);

  enterprise_connectors::ScanResult* stored_result =
      static_cast<enterprise_connectors::ScanResult*>(
          download->GetUserData(enterprise_connectors::ScanResult::kKey));
  if (stored_result) {
    for (const auto& metadata : stored_result->file_metadata) {
      router->OnDangerousDownloadEvent(
          download->GetURL(), download->GetTabUrl(), metadata.filename,
          metadata.sha256, original_danger_type, metadata.mime_type,
          enterprise_connectors::kFileDownloadDataTransferEventTrigger,
          metadata.scan_response.request_token(), metadata.size, referrer_chain,
          frame_url_chain, enterprise_connectors::EventResult::BYPASSED);
    }
  } else {
    router->OnDangerousDownloadEvent(
        download->GetURL(), download->GetTabUrl(),
        download->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(download->GetHash()), original_danger_type,
        download->GetMimeType(),
        enterprise_connectors::kFileDownloadDataTransferEventTrigger,
        /*scan_id*/ "", download->GetTotalBytes(), referrer_chain,
        frame_url_chain, enterprise_connectors::EventResult::BYPASSED);
  }
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void ReportAnalysisConnectorWarningBypassed(download::DownloadItem* download) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return;

  ReferrerChain referrer_chain;
  if (base::FeatureList::IsEnabled(kEnhancedFieldsForSecOps)) {
    referrer_chain = GetOrIdentifyReferrerChainForEnterprise(*download);
  }

  enterprise_connectors::ScanResult* stored_result =
      static_cast<enterprise_connectors::ScanResult*>(
          download->GetUserData(enterprise_connectors::ScanResult::kKey));

  enterprise_connectors::DownloadContentAreaUserProvider info(*download);
  if (stored_result) {
    for (const auto& metadata : stored_result->file_metadata) {
      ReportAnalysisConnectorWarningBypass(
          profile, info, "", "", metadata.filename, metadata.sha256,
          metadata.mime_type,
          enterprise_connectors::kFileDownloadDataTransferEventTrigger, "",
          metadata.size, referrer_chain, metadata.scan_response,
          stored_result->user_justification);
    }
  } else {
    ReportAnalysisConnectorWarningBypass(
        profile, info, "", "", download->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(download->GetHash()), download->GetMimeType(),
        enterprise_connectors::kFileDownloadDataTransferEventTrigger, "",
        download->GetTotalBytes(), referrer_chain,
        enterprise_connectors::ContentAnalysisResponse(),
        /*user_justification=*/std::nullopt);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
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

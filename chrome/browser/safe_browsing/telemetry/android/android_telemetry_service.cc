// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserContext;

namespace safe_browsing {

namespace {
// File suffix for APKs.
const base::FilePath::CharType kApkSuffix[] = FILE_PATH_LITERAL(".apk");
// MIME-type for APKs.
const char kApkMimeType[] = "application/vnd.android.package-archive";

// The number of user gestures to trace back for the referrer chain.
const int kAndroidTelemetryUserGestureLimit = 2;

void RecordApkDownloadTelemetryOutcome(ApkDownloadTelemetryOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.AndroidTelemetry.ApkDownload.Outcome",
                            outcome);
}

}  // namespace

AndroidTelemetryService::AndroidTelemetryService(Profile* profile)
    : TelemetryService(), profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          profile_->GetProfileKey());
  coordinator->AddObserver(this);
}

AndroidTelemetryService::~AndroidTelemetryService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          profile_->GetProfileKey());
  coordinator->RemoveObserver(this);
}

void AndroidTelemetryService::OnDownloadCreated(download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          profile_->GetProfileKey());
  if (!coordinator->has_all_history_downloads()) {
    return;
  }

  item->AddObserver(this);
  FillReferrerChain(item);
}

void AndroidTelemetryService::OnDownloadUpdated(download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanSendPing(item)) {
    return;
  }

  if (item->GetState() == download::DownloadItem::COMPLETE) {
    // Download completed. Send report.
    GetReport(item, base::BindOnce(&AndroidTelemetryService::OnGetReportDone,
                                   weak_ptr_factory_.GetWeakPtr(), item));
  } else if (item->GetState() == download::DownloadItem::CANCELLED) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_DOWNLOAD_CANCELLED);
    // No longer interested in this |DownloadItem| since the download did not
    // complete so remove the observer.
    item->RemoveObserver(this);
  }
}

void AndroidTelemetryService::OnGetReportDone(
    download::DownloadItem* item,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  if (reports_in_progress_.contains(item)) {
    reports_in_progress_.erase(item);
    MaybeSendApkDownloadReport(
        content::DownloadItemUtils::GetBrowserContext(item), std::move(report));
    // No longer interested in this |DownloadItem| since the report has been
    // sent so remove the observer.
    item->RemoveObserver(this);
  }
}

void AndroidTelemetryService::OnDownloadRemoved(download::DownloadItem* item) {
  referrer_chain_result_.erase(item);
  reports_in_progress_.erase(item);
}

bool AndroidTelemetryService::CanSendPing(download::DownloadItem* item) {
  if (!item->GetFileNameToReportUser().MatchesExtension(kApkSuffix) &&
      (item->GetMimeType() != kApkMimeType)) {
    // This case is not recorded since we are not interested here in finding out
    // how often people download non-APK files.
    return false;
  }

  if (!IsSafeBrowsingEnabled(*GetPrefs())) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_SAFE_BROWSING_NOT_ENABLED);
    return false;
  }

  if (profile_->IsOffTheRecord()) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_INCOGNITO);
    return false;
  }

  bool no_ping_allowed = !IsExtendedReportingEnabled(*GetPrefs());

  if (no_ping_allowed) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_UNCONSENTED);
    return false;
  }

  return true;
}

const PrefService* AndroidTelemetryService::GetPrefs() {
  return profile_->GetPrefs();
}

void AndroidTelemetryService::FillReferrerChain(download::DownloadItem* item) {
  using enum ApkDownloadTelemetryIncompleteReason;

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  content::RenderFrameHost* rfh =
      content::DownloadItemUtils::GetRenderFrameHost(item);
  if (!rfh && web_contents) {
    rfh = web_contents->GetPrimaryMainFrame();
  }

  if (rfh && rfh->GetLastCommittedURL().is_empty()) {
    rfh = rfh->GetOutermostMainFrame();
  }

  SafeBrowsingNavigationObserverManager* observer_manager =
      web_contents
          ? SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
                web_contents->GetBrowserContext())
          : nullptr;

  if (web_contents) {
    GURL intent_url = GetReferringAppInfo(web_contents).target_url;
    referrer_chain_result_[item].triggered_by_intent =
        intent_url == item->GetOriginalUrl();
  }

  if (!web_contents) {
    referrer_chain_result_[item].missing_reason = MISSING_WEB_CONTENTS;
    return;
  } else if (!SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
                 profile_->GetPrefs(),
                 g_browser_process->safe_browsing_service()) ||
             !observer_manager) {
    referrer_chain_result_[item].missing_reason =
        SB_NAVIGATION_MANAGER_NOT_READY;
    return;
  } else if (!rfh) {
    referrer_chain_result_[item].missing_reason = MISSING_RENDER_FRAME_HOST;
    return;
  }

  referrer_chain_result_[item].missing_reason =
      ApkDownloadTelemetryIncompleteReason::COMPLETE;

  std::unique_ptr<ReferrerChainData> data =
      safe_browsing::IdentifyReferrerChain(*item,
                                           kAndroidTelemetryUserGestureLimit);
  referrer_chain_result_[item].result = data->attribution_result();

  item->SetUserData(ReferrerChainData::kDownloadReferrerChainDataKey,
                    std::move(data));
}

void AndroidTelemetryService::GetReport(
    download::DownloadItem* item,
    base::OnceCallback<void(std::unique_ptr<ClientSafeBrowsingReportRequest>)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(item->IsDone());

  reports_in_progress_.insert(item);

  std::unique_ptr<ClientSafeBrowsingReportRequest> report(
      new ClientSafeBrowsingReportRequest());
  report->set_type(ClientSafeBrowsingReportRequest::APK_DOWNLOAD);
  report->set_url(item->GetOriginalUrl().spec());
  report->set_page_url(item->GetTabUrl().spec());
  report->set_locale(g_browser_process->GetApplicationLocale());

  auto* referrer_chain_data = static_cast<ReferrerChainData*>(
      item->GetUserData(ReferrerChainData::kDownloadReferrerChainDataKey));
  if (referrer_chain_data) {
    *report->mutable_referrer_chain() =
        *referrer_chain_data->GetReferrerChain();
  }

  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.ApkDownloadTelemetry",
      report->referrer_chain_size());
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.ApkDownloadTelemetry",
      referrer_chain_result_[item].result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.AndroidTelemetry.ApkDownload.IncompleteReason",
      referrer_chain_result_[item].missing_reason);
  base::UmaHistogramBoolean(
      "SafeBrowsing.AndroidTelemetry.DownloadDirectlyTriggeredByIntent",
      referrer_chain_result_[item].triggered_by_intent);

  // Fill DownloadItemInfo
  ClientSafeBrowsingReportRequest::DownloadItemInfo*
      mutable_download_item_info = report->mutable_download_item_info();
  mutable_download_item_info->set_url(item->GetURL().spec());
  mutable_download_item_info->mutable_digests()->set_sha256(item->GetHash());
  mutable_download_item_info->set_length(item->GetReceivedBytes());
  mutable_download_item_info->set_file_basename(
      item->GetTargetFilePath().BaseName().value());

  if (base::FeatureList::IsEnabled(kGooglePlayProtectInApkTelemetry)) {
    SafeBrowsingApiHandlerBridge::GetInstance().StartIsVerifyAppsEnabled(
        base::BindOnce(&AndroidTelemetryService::IsVerifyAppsEnabled,
                       weak_ptr_factory_.GetWeakPtr(), std::move(report),
                       std::move(callback)));
  } else {
    IsVerifyAppsEnabled(std::move(report), std::move(callback),
                        VerifyAppsEnabledResult::FAILED);
  }
}

void AndroidTelemetryService::IsVerifyAppsEnabled(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report,
    base::OnceCallback<void(std::unique_ptr<ClientSafeBrowsingReportRequest>)>
        callback,
    VerifyAppsEnabledResult result) {
  base::UmaHistogramEnumeration(
      "SBClientDownload.AndroidTelemetry.AppVerificationResult", result);

  if (result == VerifyAppsEnabledResult::SUCCESS_ENABLED) {
    report->mutable_client_properties()->set_app_verification_enabled(true);
  } else if (result == VerifyAppsEnabledResult::SUCCESS_NOT_ENABLED) {
    report->mutable_client_properties()->set_app_verification_enabled(false);
  }

  std::move(callback).Run(std::move(report));
}

void AndroidTelemetryService::MaybeSendApkDownloadReport(
    content::BrowserContext* browser_context,
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  PingManager::ReportThreatDetailsResult result =
      ChromePingManagerFactory::GetForBrowserContext(browser_context)
          ->ReportThreatDetails(std::move(report));

  if (result == PingManager::ReportThreatDetailsResult::SUCCESS) {
    RecordApkDownloadTelemetryOutcome(ApkDownloadTelemetryOutcome::SENT);
  } else if (result ==
             PingManager::ReportThreatDetailsResult::SERIALIZATION_ERROR) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_FAILED_TO_SERIALIZE);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Unhandled PingManager::ReportThreatDetailsResult type";
  }
}

}  // namespace safe_browsing

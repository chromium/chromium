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
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
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

enum class ApkDownloadTelemetryIncompleteReason {
  // |web_contents| was nullptr. This happens sometimes when downloads are
  // resumed but it's not clear exactly when.
  MISSING_WEB_CONTENTS = 0,
  // Navigation manager wasn't ready yet to provide the referrer chain.
  SB_NAVIGATION_MANAGER_NOT_READY = 1,
  // Full referrer chain captured.
  COMPLETE = 2,

  kMaxValue = COMPLETE,
};

void RecordApkDownloadTelemetryIncompleteReason(
    ApkDownloadTelemetryIncompleteReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.AndroidTelemetry.ApkDownload.IncompleteReason", reason);
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

void AndroidTelemetryService::OnDownloadCreated(
    download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          profile_->GetProfileKey());
  if (!coordinator->has_all_history_downloads())
    return;

  item->AddObserver(this);
}

void AndroidTelemetryService::OnDownloadUpdated(download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanSendPing(item)) {
    return;
  }

  if (item->GetState() == download::DownloadItem::COMPLETE) {
    // Download completed. Send report.
    std::unique_ptr<ClientSafeBrowsingReportRequest> report = GetReport(item);
    MaybeSendApkDownloadReport(
        content::DownloadItemUtils::GetBrowserContext(item), std::move(report));
    // No longer interested in this |DownloadItem| since the report has been
    // sent so remove the observer.
    item->RemoveObserver(this);
  } else if (item->GetState() == download::DownloadItem::CANCELLED) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_DOWNLOAD_CANCELLED);
    // No longer interested in this |DownloadItem| since the download did not
    // complete so remove the observer.
    item->RemoveObserver(this);
  }
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

void AndroidTelemetryService::FillReferrerChain(
    content::WebContents* web_contents,
    content::RenderFrameHost* rfh,
    ClientSafeBrowsingReportRequest* report) {
  if (!SafeBrowsingNavigationObserverManager::IsEnabledAndReady(
          profile_->GetPrefs(), g_browser_process->safe_browsing_service())) {
    RecordApkDownloadTelemetryIncompleteReason(
        ApkDownloadTelemetryIncompleteReason::SB_NAVIGATION_MANAGER_NOT_READY);
    return;
  }

  RecordApkDownloadTelemetryIncompleteReason(
      web_contents
          ? ApkDownloadTelemetryIncompleteReason::COMPLETE
          : ApkDownloadTelemetryIncompleteReason::MISSING_WEB_CONTENTS);
  SafeBrowsingNavigationObserverManager* observer_manager =
      web_contents
          ? SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
                web_contents->GetBrowserContext())
          : nullptr;
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      observer_manager
          ? observer_manager->IdentifyReferrerChainByRenderFrameHost(
                rfh, kAndroidTelemetryUserGestureLimit,
                report->mutable_referrer_chain())
          : SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND;

  size_t referrer_chain_length = report->referrer_chain().size();
  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.ApkDownloadTelemetry",
      referrer_chain_length);
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.ApkDownloadTelemetry", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  // Determines how many recent navigation events to append to referrer chain.
  size_t recent_navigations_to_collect =
      profile_ ? SafeBrowsingNavigationObserverManager::
                     CountOfRecentNavigationsToAppend(
                         profile_, profile_->GetPrefs(), result)
               : 0u;
  if (observer_manager) {
    observer_manager->AppendRecentNavigations(recent_navigations_to_collect,
                                              report->mutable_referrer_chain());
  }
}

std::unique_ptr<ClientSafeBrowsingReportRequest>
AndroidTelemetryService::GetReport(download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(item->IsDone());

  std::unique_ptr<ClientSafeBrowsingReportRequest> report(
      new ClientSafeBrowsingReportRequest());
  report->set_type(ClientSafeBrowsingReportRequest::APK_DOWNLOAD);
  report->set_url(item->GetOriginalUrl().spec());
  report->set_page_url(item->GetTabUrl().spec());

  // Fill referrer chain.
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  content::RenderFrameHost* rfh =
      content::DownloadItemUtils::GetRenderFrameHost(item);
  if (!rfh && web_contents)
    rfh = web_contents->GetPrimaryMainFrame();

  FillReferrerChain(web_contents, rfh, report.get());

  // Fill DownloadItemInfo
  ClientSafeBrowsingReportRequest::DownloadItemInfo*
      mutable_download_item_info = report->mutable_download_item_info();
  mutable_download_item_info->set_url(item->GetURL().spec());
  mutable_download_item_info->mutable_digests()->set_sha256(item->GetHash());
  mutable_download_item_info->set_length(item->GetReceivedBytes());
  mutable_download_item_info->set_file_basename(
      item->GetTargetFilePath().BaseName().value());

  return report;
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
    NOTREACHED() << "Unhandled PingManager::ReportThreatDetailsResult type";
  }
}

}  // namespace safe_browsing

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/ping_manager.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"

using content::BrowserContext;

namespace safe_browsing {

namespace {
// MIME-type for APKs.
const char kApkMimeType[] = "application/vnd.android.package-archive";

// The number of user gestures to trace back for the referrer chain.
const int kAndroidTelemetryUserGestureLimit = 2;

bool CanCaptureSafetyNetId() {
  return base::FeatureList::IsEnabled(safe_browsing::kCaptureSafetyNetId);
}

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

AndroidTelemetryService::AndroidTelemetryService(
    SafeBrowsingService* sb_service,
    Profile* profile)
    : TelemetryService(), profile_(profile), sb_service_(sb_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(sb_service_);

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

  if (!CanSendPing(item)) {
    return;
  }

  // The report can be sent. Try capturing the safety net ID. This should
  // complete before the download completes, but is not guaranteed, That's OK.
  MaybeCaptureSafetyNetId();

  item->AddObserver(this);
}

void AndroidTelemetryService::OnDownloadUpdated(download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(kApkMimeType, item->GetMimeType());

  if (item->GetState() == download::DownloadItem::COMPLETE) {
    // Download completed. Send report.
    std::unique_ptr<ClientSafeBrowsingReportRequest> report = GetReport(item);
    MaybeSendApkDownloadReport(std::move(report));
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
  if (item->GetMimeType() != kApkMimeType) {
    // This case is not recorded since we are not interested here in finding out
    // how often people download non-APK files.
    return false;
  }

  if (!IsSafeBrowsingEnabled()) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_SAFE_BROWSING_NOT_ENABLED);
    return false;
  }

  if (profile_->IsOffTheRecord()) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_INCOGNITO);
    return false;
  }

  if (!IsExtendedReportingEnabled(*GetPrefs())) {
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_EXTENDED_REPORTING_DISABLED);
    return false;
  }

  return true;
}

const PrefService* AndroidTelemetryService::GetPrefs() {
  return profile_->GetPrefs();
}

bool AndroidTelemetryService::IsSafeBrowsingEnabled() {
  return GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
}

void AndroidTelemetryService::FillReferrerChain(
    content::WebContents* web_contents,
    ClientSafeBrowsingReportRequest* report) {
  if (!SafeBrowsingNavigationObserverManager::IsEnabledAndReady(profile_)) {
    RecordApkDownloadTelemetryIncompleteReason(
        ApkDownloadTelemetryIncompleteReason::SB_NAVIGATION_MANAGER_NOT_READY);
    return;
  }

  RecordApkDownloadTelemetryIncompleteReason(
      web_contents
          ? ApkDownloadTelemetryIncompleteReason::COMPLETE
          : ApkDownloadTelemetryIncompleteReason::MISSING_WEB_CONTENTS);
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      sb_service_->navigation_observer_manager()
          ->IdentifyReferrerChainByWebContents(
              web_contents, kAndroidTelemetryUserGestureLimit,
              report->mutable_referrer_chain());

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
                     CountOfRecentNavigationsToAppend(*profile_, result)
               : 0u;
  sb_service_->navigation_observer_manager()->AppendRecentNavigations(
      recent_navigations_to_collect, report->mutable_referrer_chain());
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
  FillReferrerChain(web_contents, report.get());

  // Fill DownloadItemInfo
  ClientSafeBrowsingReportRequest::DownloadItemInfo*
      mutable_download_item_info = report->mutable_download_item_info();
  mutable_download_item_info->set_url(item->GetURL().spec());
  mutable_download_item_info->mutable_digests()->set_sha256(item->GetHash());
  mutable_download_item_info->set_length(item->GetReceivedBytes());
  mutable_download_item_info->set_file_basename(
      item->GetTargetFilePath().BaseName().value());
  report->set_safety_net_id(safety_net_id_on_ui_thread_);

  return report;
}

void AndroidTelemetryService::MaybeCaptureSafetyNetId() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(sb_service_->database_manager());
  if (!CanCaptureSafetyNetId() || !safety_net_id_on_ui_thread_.empty() ||
      !sb_service_->database_manager()->IsSupported()) {
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&SafeBrowsingDatabaseManager::GetSafetyNetId,
                     sb_service_->database_manager()),
      base::BindOnce(&AndroidTelemetryService::SetSafetyNetIdOnUIThread,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidTelemetryService::MaybeSendApkDownloadReport(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  std::string serialized;
  if (!report->SerializeToString(&serialized)) {
    DLOG(ERROR) << "Unable to serialize the APK download telemetry report.";
    RecordApkDownloadTelemetryOutcome(
        ApkDownloadTelemetryOutcome::NOT_SENT_FAILED_TO_SERIALIZE);
    return;
  }
  sb_service_->ping_manager()->ReportThreatDetails(serialized);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WebUIInfoSingleton::AddToCSBRRsSent,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     std::move(report)));

  RecordApkDownloadTelemetryOutcome(ApkDownloadTelemetryOutcome::SENT);
}

void AndroidTelemetryService::SetSafetyNetIdOnUIThread(const std::string& sid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  safety_net_id_on_ui_thread_ = sid;
}

}  // namespace safe_browsing

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_url_sb_client.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "components/safe_browsing/content/browser/client_report_util.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

using enum ExtendedReportingLevel;

DownloadUrlSBClient::DownloadUrlSBClient(
    download::DownloadItem* item,
    DownloadProtectionService* service,
    CheckDownloadCallback callback,
    const scoped_refptr<SafeBrowsingUIManager>& ui_manager,
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
    : item_(item),
      sha256_hash_(item->GetHash()),
      url_chain_(item->GetUrlChain()),
      referrer_url_(item->GetReferrerUrl()),
      total_type_(DOWNLOAD_URL_CHECKS_TOTAL),
      dangerous_type_(DOWNLOAD_URL_CHECKS_MALWARE),
      service_(service),
      callback_(std::move(callback)),
      ui_manager_(ui_manager),
      start_time_(base::TimeTicks::Now()),
      database_manager_(database_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(item_);
  DCHECK(service_);
  download_item_observation_.Observe(item_.get());
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  extended_reporting_level_ =
      profile ? GetExtendedReportingLevel(*profile->GetPrefs())
              : SBER_LEVEL_OFF;
  is_enhanced_protection_ =
      profile ? IsEnhancedProtectionEnabled(*profile->GetPrefs()) : false;
}

// Implements DownloadItem::Observer.
void DownloadUrlSBClient::OnDownloadDestroyed(
    download::DownloadItem* download) {
  DCHECK(download_item_observation_.IsObservingSource(item_.get()));
  download_item_observation_.Reset();
  item_ = nullptr;
}

void DownloadUrlSBClient::StartCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!database_manager_.get() ||
      database_manager_->CheckDownloadUrl(url_chain_, this)) {
    CheckDone(SBThreatType::SB_THREAT_TYPE_SAFE);
  } else {
    // Add a reference to this object to prevent it from being destroyed
    // before url checking result is returned.
    AddRef();
  }
}

bool DownloadUrlSBClient::IsDangerous(SBThreatType threat_type) const {
  return threat_type == SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE;
}

// Implements SafeBrowsingDatabaseManager::Client.
void DownloadUrlSBClient::OnCheckDownloadUrlResult(
    const std::vector<GURL>& url_chain,
    SBThreatType threat_type) {
  CheckDone(threat_type);
  UMA_HISTOGRAM_TIMES("SB2.DownloadUrlCheckDuration",
                      base::TimeTicks::Now() - start_time_);
  Release();
}

DownloadUrlSBClient::~DownloadUrlSBClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void DownloadUrlSBClient::CheckDone(SBThreatType threat_type) {
  DownloadCheckResult result = IsDangerous(threat_type)
                                   ? DownloadCheckResult::DANGEROUS
                                   : DownloadCheckResult::SAFE;
  UpdateDownloadCheckStats(total_type_);
  if (threat_type != SBThreatType::SB_THREAT_TYPE_SAFE) {
    UpdateDownloadCheckStats(dangerous_type_);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadUrlSBClient::ReportMalware, this, threat_type));
  } else {
    // Identify download referrer chain, which will be used in
    // ClientDownloadRequest.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadUrlSBClient::IdentifyReferrerChain, this));
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), result));
}

void DownloadUrlSBClient::ReportMalware(SBThreatType threat_type) {
  std::string post_data;
  if (!sha256_hash_.empty()) {
    post_data += base::HexEncode(sha256_hash_) + "\n";
  }
  for (size_t i = 0; i < url_chain_.size(); ++i) {
    post_data += url_chain_[i].spec() + "\n";
  }

  std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
  hit_report->malicious_url = url_chain_.back();
  hit_report->page_url = url_chain_.front();
  hit_report->referrer_url = referrer_url_;
  hit_report->is_subresource = true;
  hit_report->threat_type = threat_type;
  hit_report->threat_source = database_manager_->GetNonBrowseUrlThreatSource();
  hit_report->post_data = post_data;
  hit_report->extended_reporting_level = extended_reporting_level_;
  hit_report->is_enhanced_protection = is_enhanced_protection_;
  hit_report->is_metrics_reporting_active =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

  ui_manager_->MaybeReportSafeBrowsingHit(
      std::move(hit_report), content::DownloadItemUtils::GetWebContents(item_));

  if (base::FeatureList::IsEnabled(
          safe_browsing::kCreateWarningShownClientSafeBrowsingReports)) {
    CreateAndMaybeSendClientSafeBrowsingWarningShownReport(post_data);
  }
}

void DownloadUrlSBClient::IdentifyReferrerChain() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!item_)
    return;

  std::unique_ptr<ReferrerChainData> data =
      safe_browsing::IdentifyReferrerChain(
          *item_,
          DownloadProtectionService::GetDownloadAttributionUserGestureLimit(
              item_));
  if (!data) {
    return;
  }

  // TODO(drubery): Add a kMaxValue to AttributionResult so we don't need the
  // explicit max value.
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ReferrerAttributionResult.DownloadAttribution",
      data->attribution_result(),
      ReferrerChainProvider::AttributionResult::ATTRIBUTION_FAILURE_TYPE_MAX);
  item_->SetUserData(ReferrerChainData::kDownloadReferrerChainDataKey,
                     std::move(data));
}

void DownloadUrlSBClient::UpdateDownloadCheckStats(SBStatsType stat_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.DownloadChecks", stat_type,
                            DOWNLOAD_CHECKS_MAX);
}

void DownloadUrlSBClient::
    CreateAndMaybeSendClientSafeBrowsingWarningShownReport(
        std::string post_data) {
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_url(url_chain_.back().spec());
  report->set_page_url(url_chain_.front().spec());
  report->set_referrer_url(referrer_url_.spec());
  report->set_type(ClientSafeBrowsingReportRequest::WARNING_SHOWN);
  report->mutable_client_properties()->set_url_api_type(
      client_report_utils::GetUrlApiTypeForThreatSource(
          database_manager_->GetNonBrowseUrlThreatSource()));
  report->set_warning_shown_timestamp_msec(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  report->mutable_warning_shown_info()->set_warning_type(
      ClientSafeBrowsingReportRequest::WarningShownInfo::
          BINARY_MALWARE_DOWNLOAD_WARNING);
  report->mutable_warning_shown_info()->set_post_data(post_data);
  ui_manager_->MaybeSendClientSafeBrowsingWarningShownReport(
      std::move(report), content::DownloadItemUtils::GetWebContents(item_));
}

}  // namespace safe_browsing

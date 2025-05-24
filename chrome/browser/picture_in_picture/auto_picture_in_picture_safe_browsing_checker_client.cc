// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_safe_browsing_checker_client.h"

#include "content/public/browser/browser_thread.h"

AutoPictureInPictureSafeBrowsingCheckerClient::
    AutoPictureInPictureSafeBrowsingCheckerClient(
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager,
        base::TimeDelta safe_browsing_check_delay,
        ReportUrlSafetyCb report_url_safety_cb)
    : safe_browsing::SafeBrowsingDatabaseManager::Client(GetPassKey()),
      database_manager_(database_manager),
      safe_browsing_check_delay_(safe_browsing_check_delay),
      report_url_safety_cb_(std::move(report_url_safety_cb)),
      threat_types_(safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING})) {
  DCHECK_GE(safe_browsing_check_delay_, kMinimumCheckDelay);
}

AutoPictureInPictureSafeBrowsingCheckerClient::
    ~AutoPictureInPictureSafeBrowsingCheckerClient() {
  if (timer_.IsRunning()) {
    DCHECK(database_manager_);
    database_manager_->CancelCheck(this);
    timer_.Stop();
  }
}

void AutoPictureInPictureSafeBrowsingCheckerClient::CheckUrlSafety(GURL url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(database_manager_);

  if (timer_.IsRunning()) {
    database_manager_->CancelCheck(this);
    report_url_safety_cb_.Run(false);
  }

  // Set a timer to report the url is not safe, if the acynchronous check takes
  // too long.
  auto timeout_callback = base::BindOnce(
      &AutoPictureInPictureSafeBrowsingCheckerClient::OnCheckBlocklistTimeout,
      weak_factory_.GetWeakPtr());

  // Start a timer to abort the check if it takes too long.
  timer_.Start(FROM_HERE, safe_browsing_check_delay_,
               std::move(timeout_callback));

  // Check the Safe Browsing blocklists.
  DCHECK(database_manager_);
  bool is_safe_synchronously = database_manager_->CheckBrowseUrl(
      url, threat_types_, this,
      safe_browsing::CheckBrowseUrlType::kHashDatabase);

  // If the check is performed synchronously, stop the timer and immediately
  // report that the URL is safe.
  //
  // The timer is stopped to avoid `OnCheckBlocklistTimeout` from being called,
  // since when the check is performed synchronously `OnCheckBrowseUrlResult` is
  // not called.
  if (is_safe_synchronously) {
    timer_.Stop();
    report_url_safety_cb_.Run(true);
  }
}

void AutoPictureInPictureSafeBrowsingCheckerClient::OnCheckBrowseUrlResult(
    const GURL& url,
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop the timer to avoid calling `OnCheckBlocklistTimeout`, since we got a
  // blocklist check result in time.
  timer_.Stop();

  if (threat_types_.contains(threat_type)) {
    report_url_safety_cb_.Run(false);
    return;
  }

  report_url_safety_cb_.Run(true);
}

void AutoPictureInPictureSafeBrowsingCheckerClient::OnCheckBlocklistTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(database_manager_);

  database_manager_->CancelCheck(this);
  report_url_safety_cb_.Run(false);
}

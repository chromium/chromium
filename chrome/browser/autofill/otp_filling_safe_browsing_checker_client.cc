// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/otp_filling_safe_browsing_checker_client.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_thread.h"

namespace autofill {

// static
std::unique_ptr<OtpFillingSafeBrowsingCheckerClient>
OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
        v5_get_hash_protocol_manager,
    base::TimeDelta safe_browsing_check_delay,
    const GURL& main_frame_url,
    const GURL& frame_to_fill_url,
    ResultCallback callback) {
  auto client = base::WrapUnique(new OtpFillingSafeBrowsingCheckerClient(
      std::move(database_manager), v5_get_hash_protocol_manager,
      safe_browsing_check_delay, std::move(callback)));
  client->CheckUrlSafety(main_frame_url, frame_to_fill_url);
  return client;
}

OtpFillingSafeBrowsingCheckerClient::OtpFillingSafeBrowsingCheckerClient(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
        v5_get_hash_protocol_manager,
    base::TimeDelta safe_browsing_check_delay,
    ResultCallback callback)
    : safe_browsing::SafeBrowsingDatabaseManager::Client(GetPassKey()),
      database_manager_(database_manager),
      v5_get_hash_protocol_manager_(v5_get_hash_protocol_manager),
      safe_browsing_check_delay_(safe_browsing_check_delay),
      callback_(std::move(callback)),
      threat_types_(safe_browsing::CreateSBThreatTypeSet(
          {safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE,
           safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED,
#if !BUILDFLAG(IS_ANDROID)
           safe_browsing::SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE,
#endif  // !BUILDFLAG(IS_ANDROID)
           safe_browsing::SBThreatType::SB_THREAT_TYPE_BILLING})) {
}

OtpFillingSafeBrowsingCheckerClient::~OtpFillingSafeBrowsingCheckerClient() {
  if (timer_.IsRunning()) {
    CHECK(database_manager_);
    database_manager_->CancelCheck(this);
    timer_.Stop();
  }
}

void OtpFillingSafeBrowsingCheckerClient::CheckUrlSafety(
    const GURL& main_frame_url,
    const GURL& frame_to_fill_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(database_manager_);
  CHECK(!timer_.IsRunning() && callback_)
      << "OtpFillingSafeBrowsingCheckerClient is strictly single-use per "
         "check.";

  urls_to_check_.push_back(main_frame_url);
  if (frame_to_fill_url != main_frame_url) {
    urls_to_check_.push_back(frame_to_fill_url);
  }
  current_url_index_ = 0;

  CheckNextUrl();
}

void OtpFillingSafeBrowsingCheckerClient::CheckNextUrl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (current_url_index_ >= urls_to_check_.size()) {
    // Note: This callback is always executed asynchronously relative to
    // `CheckUrlSafety`, as `CheckNextUrl` is always called asynchronously.
    RunCallback(/*is_malicious=*/false);
    return;
  }

  const GURL& url = urls_to_check_[current_url_index_];

  // Start a timer to report the url is malicious/unsafe, if the asynchronous
  // check takes too long.
  timer_.Start(
      FROM_HERE, safe_browsing_check_delay_,
      base::BindOnce(
          &OtpFillingSafeBrowsingCheckerClient::OnCheckBlocklistTimeout,
          weak_factory_.GetWeakPtr()));

  // `CheckBrowseUrl` returns true if the check completes synchronously and the
  // URL is safe (not in the blocklist). It returns false if the check is
  // proceeding asynchronously, in which case `OnCheckBrowseUrlResult` will be
  // invoked when the asynchronous check finishes.
  bool is_safe_synchronously = database_manager_->CheckBrowseUrl(
      url, threat_types_, this,
      safe_browsing::CheckBrowseUrlType::kHashDatabase);

  if (is_safe_synchronously) {
    timer_.Stop();
    current_url_index_++;
    // Use PostTask to advance to the next URL to avoid synchronous recursion
    // or reentrancy when `CheckBrowseUrl` returns true.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&OtpFillingSafeBrowsingCheckerClient::CheckNextUrl,
                       weak_factory_.GetWeakPtr()));
  }
}

void OtpFillingSafeBrowsingCheckerClient::OnCheckBrowseUrlResult(
    const GURL& url,
    safe_browsing::SBThreatType threat_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  timer_.Stop();

  if (threat_types_.contains(threat_type)) {
    RunCallback(/*is_malicious=*/true);
    return;
  }

  current_url_index_++;
  // Use PostTask to advance to the next URL to avoid unexpected stack depth or
  // reentrancy issues if a database manager implementation invokes the callback
  // synchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OtpFillingSafeBrowsingCheckerClient::CheckNextUrl,
                     weak_factory_.GetWeakPtr()));
}

base::WeakPtr<safe_browsing::V5GetHashProtocolManager>
OtpFillingSafeBrowsingCheckerClient::GetV5GetHashProtocolManager() {
  return v5_get_hash_protocol_manager_;
}

void OtpFillingSafeBrowsingCheckerClient::OnCheckBlocklistTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(database_manager_);

  database_manager_->CancelCheck(this);
  RunCallback(/*is_malicious=*/true);
}

void OtpFillingSafeBrowsingCheckerClient::RunCallback(bool is_malicious) {
  urls_to_check_.clear();
  CHECK(callback_);
  // Move `callback_` into a local variable before running it, as running the
  // callback may destroy `this`.
  auto callback = std::move(callback_);
  std::move(callback).Run(is_malicious);
}

}  // namespace autofill

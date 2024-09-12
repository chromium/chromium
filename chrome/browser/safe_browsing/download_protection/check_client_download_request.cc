// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

namespace {

// This function is called when the result of malware scanning is already known
// (via |reason|), but we still want to perform DLP scanning.
void MaybeOverrideScanResult(DownloadCheckResultReason reason,
                             CheckDownloadRepeatingCallback callback,
                             DownloadCheckResult deep_scan_result) {
  switch (deep_scan_result) {
    // These results are more dangerous or equivalent to any |reason|, so they
    // take precedence.
    case DownloadCheckResult::DANGEROUS_HOST:
    case DownloadCheckResult::DANGEROUS:
    case DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE:
      callback.Run(deep_scan_result);
      return;

    // These deep scanning results don't override any dangerous reasons.
    case DownloadCheckResult::UNKNOWN:
    case DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
    case DownloadCheckResult::DEEP_SCANNED_SAFE:
    case DownloadCheckResult::DEEP_SCANNED_FAILED:
    case DownloadCheckResult::SAFE:
    case DownloadCheckResult::PROMPT_FOR_SCANNING:
    case DownloadCheckResult::PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case DownloadCheckResult::POTENTIALLY_UNWANTED:
    case DownloadCheckResult::UNCOMMON:
    case DownloadCheckResult::IMMEDIATE_DEEP_SCAN:
      if (reason == REASON_DOWNLOAD_DANGEROUS) {
        callback.Run(DownloadCheckResult::DANGEROUS);
      } else if (reason == REASON_DOWNLOAD_DANGEROUS_HOST) {
        callback.Run(DownloadCheckResult::DANGEROUS_HOST);
      } else if (reason == REASON_DOWNLOAD_POTENTIALLY_UNWANTED) {
        callback.Run(DownloadCheckResult::POTENTIALLY_UNWANTED);
      } else if (reason == REASON_DOWNLOAD_UNCOMMON) {
        callback.Run(DownloadCheckResult::UNCOMMON);
      } else if (reason == REASON_DOWNLOAD_DANGEROUS_ACCOUNT_COMPROMISE) {
        callback.Run(DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE);
      } else {
        callback.Run(deep_scan_result);
      }
      return;

    // These other results have precedence over dangerous ones because they
    // indicate the scan is not done, that the file is blocked for another
    // reason, or that the file is allowed by policy.
    case DownloadCheckResult::ASYNC_SCANNING:
    case DownloadCheckResult::ASYNC_LOCAL_PASSWORD_SCANNING:
    case DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
    case DownloadCheckResult::BLOCKED_TOO_LARGE:
    case DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
    case DownloadCheckResult::ALLOWLISTED_BY_POLICY:
    case DownloadCheckResult::BLOCKED_SCAN_FAILED:
      callback.Run(deep_scan_result);
      return;
  }

  // This function should always run |callback| and return before reaching this.
  CHECK(false);
}

void LogNoticeSeenMetrics(PrefService* prefs) {
  bool has_seen =
      prefs->GetBoolean(prefs::kSafeBrowsingAutomaticDeepScanningIPHSeen);
  if (prefs->GetBoolean(prefs::kDownloadBubblePartialViewEnabled)) {
    base::UmaHistogramBoolean(
        "SBClientDownload.AutomaticDeepScanNoticeSeen2.PartialViewEnabled",
        has_seen);
  } else {
    base::UmaHistogramBoolean(
        "SBClientDownload.AutomaticDeepScanNoticeSeen2.PartialViewSuppressed",
        has_seen);
  }
}

bool ShouldUploadToDownloadFeedback(DownloadCheckResult result) {
  switch (result) {
    case DownloadCheckResult::DANGEROUS_HOST:
    case DownloadCheckResult::DANGEROUS:
    case DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE:
    case DownloadCheckResult::POTENTIALLY_UNWANTED:
    case DownloadCheckResult::UNCOMMON:
    case DownloadCheckResult::UNKNOWN:
      return true;

    case DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
    case DownloadCheckResult::DEEP_SCANNED_SAFE:
    case DownloadCheckResult::DEEP_SCANNED_FAILED:
    case DownloadCheckResult::SAFE:
    case DownloadCheckResult::PROMPT_FOR_SCANNING:
    case DownloadCheckResult::PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case DownloadCheckResult::IMMEDIATE_DEEP_SCAN:
    case DownloadCheckResult::ASYNC_SCANNING:
    case DownloadCheckResult::ASYNC_LOCAL_PASSWORD_SCANNING:
    case DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
    case DownloadCheckResult::BLOCKED_TOO_LARGE:
    case DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
    case DownloadCheckResult::ALLOWLISTED_BY_POLICY:
    case DownloadCheckResult::BLOCKED_SCAN_FAILED:
      return false;
  }
}

}  // namespace

using content::BrowserThread;

CheckClientDownloadRequest::CheckClientDownloadRequest(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    base::optional_ref<const std::string> password)
    : CheckClientDownloadRequestBase(
          item->GetURL(),
          item->GetTargetFilePath(),
          content::DownloadItemUtils::GetBrowserContext(item),
          callback,
          service,
          std::move(database_manager),
          DownloadRequestMaker::CreateFromDownloadItem(binary_feature_extractor,
                                                       item,
                                                       password)),
      item_(item),
      password_(password.CopyAsOptional()),
      callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->AddObserver(this);
  DVLOG(2) << "Starting SafeBrowsing download check for: "
           << item_->DebugString(true);
}

void CheckClientDownloadRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  FinishRequest(DownloadCheckResult::UNKNOWN, REASON_DOWNLOAD_DESTROYED);
}

void CheckClientDownloadRequest::OnDownloadUpdated(
    download::DownloadItem* download) {
  // Consider the scan bypassed if the user clicked "Open now" or "Cancel"
  // before scanning is done.
  if (download->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING &&
      (download->GetState() == download::DownloadItem::COMPLETE ||
       download->GetState() == download::DownloadItem::CANCELLED)) {
    auto settings = DeepScanningRequest::ShouldUploadBinary(item_);
    if (settings.has_value()) {
      RecordDeepScanMetrics(
          settings->cloud_or_local_settings.is_cloud_analysis(),
          /*access_point=*/DeepScanAccessPoint::DOWNLOAD,
          /*duration=*/base::TimeTicks::Now() - upload_start_time_,
          /*total_size=*/item_->GetTotalBytes(),
          /*result=*/"BypassedByUser",
          /*failure=*/true);
    }
  }
}

// static
bool CheckClientDownloadRequest::IsSupportedDownload(
    const download::DownloadItem& item,
    const base::FilePath& target_path,
    DownloadCheckResultReason* reason) {
  if (item.GetUrlChain().empty()) {
    *reason = REASON_EMPTY_URL_CHAIN;
    return false;
  }
  const GURL& final_url = item.GetUrlChain().back();
  if (!final_url.is_valid() || final_url.is_empty()) {
    *reason = REASON_INVALID_URL;
    return false;
  }
  if (!final_url.IsStandard() && !final_url.SchemeIsBlob() &&
      !final_url.SchemeIs(url::kDataScheme)) {
    *reason = REASON_UNSUPPORTED_URL_SCHEME;
    return false;
  }
  // TODO(crbug.com/41372015): Remove duplicated counting of REMOTE_FILE
  // and LOCAL_FILE in SBClientDownload.UnsupportedScheme.*.
  if (final_url.SchemeIsFile()) {
    *reason = final_url.has_host() ? REASON_REMOTE_FILE : REASON_LOCAL_FILE;
    return false;
  }
  // This check should be last, so we know the earlier checks passed.
  if (!FileTypePolicies::GetInstance()->IsCheckedBinaryFile(target_path)) {
    *reason = REASON_NOT_BINARY_FILE;
    return false;
  }
  return true;
}

CheckClientDownloadRequest::~CheckClientDownloadRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->RemoveObserver(this);
}

bool CheckClientDownloadRequest::IsSupportedDownload(
    DownloadCheckResultReason* reason) {
  return IsSupportedDownload(*item_, item_->GetTargetFilePath(), reason);
}

download::DownloadItem* CheckClientDownloadRequest::item() const {
  return item_;
}

content::BrowserContext* CheckClientDownloadRequest::GetBrowserContext() const {
  return content::DownloadItemUtils::GetBrowserContext(item_);
}

bool CheckClientDownloadRequest::IsCancelled() {
  return item_->GetState() == download::DownloadItem::CANCELLED;
}

base::WeakPtr<CheckClientDownloadRequestBase>
CheckClientDownloadRequest::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CheckClientDownloadRequest::NotifySendRequest(
    const ClientDownloadRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service()->client_download_request_callbacks_.Notify(item_, request);
  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.DownloadAttribution",
      request->referrer_chain().size());
}

void CheckClientDownloadRequest::SetDownloadProtectionData(
    const std::string& token,
    const ClientDownloadResponse::Verdict& verdict,
    const ClientDownloadResponse::TailoredVerdict& tailored_verdict) {
  DCHECK(!token.empty());
  DownloadProtectionService::SetDownloadProtectionData(item_, token, verdict,
                                                       tailored_verdict);
}

void CheckClientDownloadRequest::MaybeBeginFeedbackForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    const std::string& request_data,
    const std::string& response_body) {
  if (!upload_requested) {
    return;
  }

  if (item_->GetReceivedBytes() > DownloadFeedback::kMaxUploadSize) {
    return;
  }

  if (ShouldUploadToDownloadFeedback(result) && !item_->IsInsecure()) {
    Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
    service()->MaybeBeginFeedbackForDownload(profile, item_, request_data,
                                             response_body);
  }
}

void CheckClientDownloadRequest::LogDeepScanningPrompt(bool did_prompt) const {
  if (did_prompt) {
    LogDeepScanEvent(item_, DeepScanEvent::kPromptShown);
  }

  base::UmaHistogramBoolean("SBClientDownload.ServerRequestsDeepScanningPrompt",
                            did_prompt);
  if (DownloadItemWarningData::IsTopLevelEncryptedArchive(item_)) {
    base::UmaHistogramBoolean(
        "SBClientDownload.ServerRequestsDeepScanningPromptPasswordProtected",
        did_prompt);
  }
}

std::optional<enterprise_connectors::AnalysisSettings>
CheckClientDownloadRequest::ShouldUploadBinary(
    DownloadCheckResultReason reason) {
  // If the download was destroyed, we can't upload it.
  if (reason == REASON_DOWNLOAD_DESTROYED) {
    return std::nullopt;
  }

  // If the download already has a scanning response attached, there is no need
  // to try and upload it again.
  if (item_->GetUserData(enterprise_connectors::ScanResult::kKey)) {
    return std::nullopt;
  }

  // If the download is considered dangerous, don't upload the binary to show
  // a warning to the user ASAP.
  if (reason == REASON_DOWNLOAD_DANGEROUS ||
      reason == REASON_DOWNLOAD_DANGEROUS_HOST ||
      reason == REASON_DOWNLOAD_DANGEROUS_ACCOUNT_COMPROMISE) {
    return std::nullopt;
  }

  auto settings = DeepScanningRequest::ShouldUploadBinary(item_);

  // Malware scanning is redundant if the URL is allowlisted, but DLP scanning
  // might still need to happen.
  if (settings && reason == REASON_ALLOWLISTED_URL) {
    settings->tags.erase("malware");
    if (settings->tags.empty()) {
      return std::nullopt;
    }
  }

  return settings;
}

void CheckClientDownloadRequest::UploadBinary(
    DownloadCheckResult result,
    DownloadCheckResultReason reason,
    enterprise_connectors::AnalysisSettings settings) {
  if (reason == REASON_DOWNLOAD_DANGEROUS ||
      reason == REASON_DOWNLOAD_DANGEROUS_HOST ||
      reason == REASON_DOWNLOAD_POTENTIALLY_UNWANTED ||
      reason == REASON_DOWNLOAD_UNCOMMON || reason == REASON_ALLOWLISTED_URL ||
      reason == REASON_DOWNLOAD_DANGEROUS_ACCOUNT_COMPROMISE) {
    service()->UploadForDeepScanning(
        item_, base::BindRepeating(&MaybeOverrideScanResult, reason, callback_),
        DownloadItemWarningData::DeepScanTrigger::TRIGGER_POLICY, result,
        std::move(settings), /*password=*/std::nullopt);
  } else {
    service()->UploadForDeepScanning(
        item_, callback_,
        DownloadItemWarningData::DeepScanTrigger::TRIGGER_POLICY, result,
        std::move(settings), /*password=*/std::nullopt);
  }
}

void CheckClientDownloadRequest::NotifyRequestFinished(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weakptr_factory_.InvalidateWeakPtrs();

  DVLOG(2) << "SafeBrowsing download verdict for: " << item_->DebugString(true)
           << " verdict:" << reason << " result:" << static_cast<int>(result);

  item_->RemoveObserver(this);
}

bool CheckClientDownloadRequest::IsUnderAdvancedProtection(
    Profile* profile) const {
  if (!profile) {
    return false;
  }
  AdvancedProtectionStatusManager* advanced_protection_status_manager =
      AdvancedProtectionStatusManagerFactory::GetForProfile(profile);
  if (!advanced_protection_status_manager) {
    return false;
  }
  return advanced_protection_status_manager->IsUnderAdvancedProtection();
}

bool CheckClientDownloadRequest::ShouldImmediatelyDeepScan(
    bool server_requests_prompt,
    bool log_metrics) const {
  if (!ShouldPromptForDeepScanning(server_requests_prompt)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
  if (!profile) {
    return false;
  }

  if (!IsEnhancedProtectionEnabled(*profile->GetPrefs())) {
    return false;
  }

  if (DownloadItemWarningData::IsTopLevelEncryptedArchive(item_)) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(kDeepScanningPromptRemoval)) {
    return false;
  }

  if (log_metrics) {
    LogNoticeSeenMetrics(profile->GetPrefs());
  }
  if (!profile->GetPrefs()->GetBoolean(
          prefs::kSafeBrowsingAutomaticDeepScanningIPHSeen)) {
    return false;
  }

  return true;
}

bool CheckClientDownloadRequest::ShouldPromptForDeepScanning(
    bool server_requests_prompt) const {
  if (!server_requests_prompt) {
    return false;
  }

  // Too large uploads would fail immediately, so don't prompt in this case.
  if (static_cast<size_t>(item_->GetTotalBytes()) >=
      BinaryUploadService::kMaxUploadSizeBytes) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
  if (!profile) {
    return false;
  }

  if (!AreDeepScansAllowedByPolicy(*profile->GetPrefs())) {
    return false;
  }

  if (profile->IsOffTheRecord()) {
    return false;
  }

  if (IsUnderAdvancedProtection(profile) ||
      IsEnhancedProtectionEnabled(*profile->GetPrefs())) {
    return true;
  }

  return false;
}

bool CheckClientDownloadRequest::ShouldPromptForLocalDecryption(
    bool server_requests_prompt) const {
  if (!server_requests_prompt) {
    return false;
  }

  if (!DownloadItemWarningData::IsTopLevelEncryptedArchive(item_)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
  if (!profile) {
    return false;
  }

  // While this isn't a "deep" scan, enterprise customers may have similar
  // reactions to it, so we use the same policy to control it.
  if (!AreDeepScansAllowedByPolicy(*profile->GetPrefs())) {
    return false;
  }

  if (GetSafeBrowsingState(*profile->GetPrefs()) !=
      SafeBrowsingState::STANDARD_PROTECTION) {
    return false;
  }

  // Too large archive extraction would fail immediately, so don't prompt in
  // this case.
  if (static_cast<size_t>(item_->GetTotalBytes()) >=
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze(
          item_->GetTargetFilePath())) {
    return false;
  }

  return true;
}

bool CheckClientDownloadRequest::ShouldPromptForIncorrectPassword() const {
  return password_.has_value() &&
         DownloadItemWarningData::HasShownLocalDecryptionPrompt(item_) &&
         DownloadItemWarningData::HasIncorrectPassword(item_);
}

bool CheckClientDownloadRequest::ShouldShowScanFailure() const {
  return DownloadItemWarningData::HasShownLocalDecryptionPrompt(item_) &&
         !DownloadItemWarningData::IsFullyExtractedArchive(item_);
}

bool CheckClientDownloadRequest::IsAllowlistedByPolicy() const {
  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
  if (!profile) {
    return false;
  }
  return MatchesEnterpriseAllowlist(*profile->GetPrefs(), item_->GetUrlChain());
}

}  // namespace safe_browsing

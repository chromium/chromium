// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

using content::BrowserThread;
using policy::BrowserDMTokenStorage;

namespace {

// TODO(drubery): This function would be simpler if the ClientDownloadResponse
// and MalwareDeepScanningVerdict used the same enum.
std::string MalwareVerdictToThreatType(
    MalwareDeepScanningVerdict::Verdict verdict) {
  switch (verdict) {
    case MalwareDeepScanningVerdict::CLEAN:
      return "SAFE";
    case MalwareDeepScanningVerdict::UWS:
      return "POTENTIALLY_UNWANTED";
    case MalwareDeepScanningVerdict::MALWARE:
      return "DANGEROUS";
    case MalwareDeepScanningVerdict::VERDICT_UNSPECIFIED:
    default:
      return "UNKNOWN";
  }
}

void DeepScanningClientResponseToDownloadCheckResult(
    const DeepScanningClientResponse& response,
    DownloadCheckResult* download_result,
    DownloadCheckResultReason* download_reason) {
  if (response.has_malware_scan_verdict() &&
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::MALWARE) {
    *download_result = DownloadCheckResult::DANGEROUS;
    *download_reason = DownloadCheckResultReason::REASON_DOWNLOAD_DANGEROUS;
    return;
  }

  if (response.has_malware_scan_verdict() &&
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::UWS) {
    *download_result = DownloadCheckResult::POTENTIALLY_UNWANTED;
    *download_reason =
        DownloadCheckResultReason::REASON_DOWNLOAD_POTENTIALLY_UNWANTED;
    return;
  }

  if (response.has_dlp_scan_verdict()) {
    bool should_dlp_block = std::any_of(
        response.dlp_scan_verdict().triggered_rules().begin(),
        response.dlp_scan_verdict().triggered_rules().end(),
        [](const DlpDeepScanningVerdict::TriggeredRule& rule) {
          return rule.action() == DlpDeepScanningVerdict::TriggeredRule::BLOCK;
        });
    if (should_dlp_block) {
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_BLOCK;
      *download_reason =
          DownloadCheckResultReason::REASON_SENSITIVE_CONTENT_BLOCK;
      return;
    }

    bool should_dlp_warn = std::any_of(
        response.dlp_scan_verdict().triggered_rules().begin(),
        response.dlp_scan_verdict().triggered_rules().end(),
        [](const DlpDeepScanningVerdict::TriggeredRule& rule) {
          return rule.action() == DlpDeepScanningVerdict::TriggeredRule::WARN;
        });
    if (should_dlp_warn) {
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_WARNING;
      *download_reason =
          DownloadCheckResultReason::REASON_SENSITIVE_CONTENT_WARNING;
      return;
    }
  }

  *download_result = DownloadCheckResult::DEEP_SCANNED_SAFE;
  *download_reason = DownloadCheckResultReason::REASON_DEEP_SCANNED_SAFE;
}

}  // namespace

void MaybeReportDeepScanningVerdict(Profile* profile,
                                    const GURL& url,
                                    const std::string& file_name,
                                    const std::string& download_digest_sha256,
                                    const std::string& mime_type,
                                    const std::string& trigger,
                                    const int64_t content_size,
                                    BinaryUploadService::Result result,
                                    DeepScanningClientResponse response) {
  if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnLargeUnscannedFileEvent(url, file_name, download_digest_sha256,
                                    mime_type, trigger, content_size);
  }

  if (result != BinaryUploadService::Result::SUCCESS)
    return;

  if (response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::UWS ||
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::MALWARE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnDangerousDeepScanningResult(
            url, file_name, download_digest_sha256,
            MalwareVerdictToThreatType(
                response.malware_scan_verdict().verdict()),
            mime_type, trigger, content_size);
  }

  if (response.dlp_scan_verdict().status() == DlpDeepScanningVerdict::SUCCESS) {
    if (!response.dlp_scan_verdict().triggered_rules().empty()) {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
          ->OnSensitiveDataEvent(response.dlp_scan_verdict(), url, file_name,
                                 download_digest_sha256, mime_type, trigger,
                                 content_size);
    }
  }
}

CheckClientDownloadRequest::CheckClientDownloadRequest(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
    : CheckClientDownloadRequestBase(
          item->GetURL(),
          item->GetTargetFilePath(),
          item->GetFullPath(),
          {item->GetTabUrl(), item->GetTabReferrerUrl()},
          item->GetReceivedBytes(),
          content::DownloadItemUtils::GetBrowserContext(item),
          callback,
          service,
          std::move(database_manager),
          std::move(binary_feature_extractor)),
      item_(item),
      callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->AddObserver(this);
  DVLOG(2) << "Starting SafeBrowsing download check for: "
           << item_->DebugString(true);
}

// download::DownloadItem::Observer implementation.
void CheckClientDownloadRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  FinishRequest(DownloadCheckResult::UNKNOWN, REASON_DOWNLOAD_DESTROYED);
}

// static
bool CheckClientDownloadRequest::IsSupportedDownload(
    const download::DownloadItem& item,
    const base::FilePath& target_path,
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
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
  // TODO(crbug.com/814813): Remove duplicated counting of REMOTE_FILE
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
  *type = download_type_util::GetDownloadType(target_path);
  return true;
}

CheckClientDownloadRequest::~CheckClientDownloadRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->RemoveObserver(this);
}

bool CheckClientDownloadRequest::IsSupportedDownload(
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
  return IsSupportedDownload(*item_, item_->GetTargetFilePath(), reason, type);
}

content::BrowserContext* CheckClientDownloadRequest::GetBrowserContext() {
  return content::DownloadItemUtils::GetBrowserContext(item_);
}

bool CheckClientDownloadRequest::IsCancelled() {
  return item_->GetState() == download::DownloadItem::CANCELLED;
}

void CheckClientDownloadRequest::PopulateRequest(
    ClientDownloadRequest* request) {
  request->mutable_digests()->set_sha256(item_->GetHash());
  request->set_length(item_->GetReceivedBytes());
  for (size_t i = 0; i < item_->GetUrlChain().size(); ++i) {
    ClientDownloadRequest::Resource* resource = request->add_resources();
    resource->set_url(SanitizeUrl(item_->GetUrlChain()[i]));
    if (i == item_->GetUrlChain().size() - 1) {
      // The last URL in the chain is the download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      resource->set_referrer(SanitizeUrl(item_->GetReferrerUrl()));
      DVLOG(2) << "dl url " << resource->url();
      if (!item_->GetRemoteAddress().empty()) {
        resource->set_remote_ip(item_->GetRemoteAddress());
        DVLOG(2) << "  dl url remote addr: " << resource->remote_ip();
      }
      DVLOG(2) << "dl referrer " << resource->referrer();
    } else {
      DVLOG(2) << "dl redirect " << i << " " << resource->url();
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    }
  }

  request->set_user_initiated(item_->HasUserGesture());

  auto* referrer_chain_data = static_cast<ReferrerChainData*>(
      item_->GetUserData(ReferrerChainData::kDownloadReferrerChainDataKey));
  if (referrer_chain_data &&
      !referrer_chain_data->GetReferrerChain()->empty()) {
    request->mutable_referrer_chain()->Swap(
        referrer_chain_data->GetReferrerChain());
    request->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data->recent_navigations_to_collect());
    UMA_HISTOGRAM_COUNTS_100(
        "SafeBrowsing.ReferrerURLChainSize.DownloadAttribution",
        referrer_chain_data->referrer_chain_length());
  }
}

base::WeakPtr<CheckClientDownloadRequestBase>
CheckClientDownloadRequest::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CheckClientDownloadRequest::NotifySendRequest(
    const ClientDownloadRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service()->client_download_request_callbacks_.Notify(item_, request);
}

void CheckClientDownloadRequest::SetDownloadPingToken(
    const std::string& token) {
  DCHECK(!token.empty());
  DownloadProtectionService::SetDownloadPingToken(item_, token);
}

void CheckClientDownloadRequest::MaybeStorePingsForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    const std::string& request_data,
    const std::string& response_body) {
  DownloadFeedbackService::MaybeStorePingsForDownload(
      result, upload_requested, item_, request_data, response_body);
}

bool CheckClientDownloadRequest::MaybeReturnAsynchronousVerdict(
    DownloadCheckResultReason reason) {
  if (ShouldUploadBinary(reason)) {
    callback_.Run(DownloadCheckResult::ASYNC_SCANNING);
    return true;
  }

  return false;
}

bool CheckClientDownloadRequest::ShouldUploadBinary(
    DownloadCheckResultReason reason) {
  bool upload_for_dlp = ShouldUploadForDlpScan();
  bool upload_for_malware = ShouldUploadForMalwareScan(reason);
  if (!upload_for_dlp && !upload_for_malware)
    return false;

  return !!Profile::FromBrowserContext(GetBrowserContext());
}

void CheckClientDownloadRequest::UploadBinary(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  saved_result_ = result;
  saved_reason_ = reason;

  bool upload_for_dlp = ShouldUploadForDlpScan();
  bool upload_for_malware = ShouldUploadForMalwareScan(reason);
  auto request = std::make_unique<DownloadItemRequest>(
      item_, /*read_immediately=*/true,
      base::BindOnce(&CheckClientDownloadRequest::OnDeepScanningComplete,
                     weakptr_factory_.GetWeakPtr()));

  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());

  if (upload_for_dlp) {
    DlpDeepScanningClientRequest dlp_request;
    dlp_request.set_content_source(DlpDeepScanningClientRequest::FILE_DOWNLOAD);
    request->set_request_dlp_scan(std::move(dlp_request));
  }

  if (upload_for_malware) {
    MalwareDeepScanningClientRequest malware_request;
    malware_request.set_population(
        MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
    malware_request.set_download_token(
        DownloadProtectionService::GetDownloadPingToken(item_));
    request->set_request_malware_scan(std::move(malware_request));
  }

  auto dm_token = BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken();
  DCHECK(dm_token.is_valid());
  request->set_dm_token(dm_token.value());

  service()->UploadForDeepScanning(profile, std::move(request));
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

bool CheckClientDownloadRequest::ShouldUploadForDlpScan() {
  if (!base::FeatureList::IsEnabled(kDeepScanningOfDownloads))
    return false;

  int check_content_compliance = g_browser_process->local_state()->GetInteger(
      prefs::kCheckContentCompliance);
  if (check_content_compliance !=
          CheckContentComplianceValues::CHECK_DOWNLOADS &&
      check_content_compliance !=
          CheckContentComplianceValues::CHECK_UPLOADS_AND_DOWNLOADS)
    return false;

  // If there's no valid DM token, the upload will fail, so we can skip
  // uploading now.
  if (!BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken().is_valid())
    return false;

  const base::ListValue* domains = g_browser_process->local_state()->GetList(
      prefs::kURLsToCheckComplianceOfDownloadedContent);
  url_matcher::URLMatcher matcher;
  policy::url_util::AddAllowFilters(&matcher, domains);
  return !matcher.MatchURL(item_->GetURL()).empty();
}

bool CheckClientDownloadRequest::ShouldUploadForMalwareScan(
    DownloadCheckResultReason reason) {
  if (!base::FeatureList::IsEnabled(kDeepScanningOfDownloads))
    return false;

  // If we know the file is malicious, we don't need to upload it.
  if (reason != DownloadCheckResultReason::REASON_DOWNLOAD_SAFE &&
      reason != DownloadCheckResultReason::REASON_DOWNLOAD_UNCOMMON &&
      reason != DownloadCheckResultReason::REASON_VERDICT_UNKNOWN)
    return false;

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item_);
  if (!browser_context)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  int send_files_for_malware_check = profile->GetPrefs()->GetInteger(
      prefs::kSafeBrowsingSendFilesForMalwareCheck);
  if (send_files_for_malware_check !=
          SendFilesForMalwareCheckValues::SEND_DOWNLOADS &&
      send_files_for_malware_check !=
          SendFilesForMalwareCheckValues::SEND_UPLOADS_AND_DOWNLOADS)
    return false;

  // If there's no valid DM token, the upload will fail, so we can skip
  // uploading now.
  return BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken().is_valid();
}

void CheckClientDownloadRequest::OnDeepScanningComplete(
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
  if (profile) {
    std::string raw_digest_sha256 = item_->GetHash();
    MaybeReportDeepScanningVerdict(
        profile, item_->GetURL(), item_->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
        item_->GetMimeType(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
        item_->GetTotalBytes(), result, response);
  }

  // In case of error, restore the original result and reason from the server.
  DownloadCheckResult download_result = saved_result_;
  DownloadCheckResultReason download_reason = saved_reason_;

  if (result == BinaryUploadService::Result::SUCCESS) {
    DeepScanningClientResponseToDownloadCheckResult(response, &download_result,
                                                    &download_reason);
  }

  // If we're not delaying verdicts, we already ran |callback_| with the final
  // result in FinishRequest.
  callback_.Run(download_result);
  NotifyRequestFinished(download_result, download_reason);
  service()->RequestFinished(this);
}

}  // namespace safe_browsing

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_file_system_access_write_request.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/navigation_entry.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#endif

namespace safe_browsing {

using content::BrowserThread;

CheckFileSystemAccessWriteRequest::CheckFileSystemAccessWriteRequest(
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
    CheckDownloadCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
    : CheckClientDownloadRequestBase(
          GetFileSystemAccessDownloadUrl(item->frame_url),
          item->target_file_path,
          item->browser_context,
          std::move(callback),
          service,
          std::move(database_manager),
          DownloadRequestMaker::CreateFromFileSystemAccess(
              binary_feature_extractor,
              *item)),
      referrer_chain_data_(IdentifyReferrerChain(
          *item,
          DownloadProtectionService::GetDownloadAttributionUserGestureLimit())),
      metadata_(std::make_unique<FileSystemAccessMetadata>(std::move(item))),
      weak_metadata_(metadata_->GetWeakPtr()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CheckFileSystemAccessWriteRequest ::~CheckFileSystemAccessWriteRequest() =
    default;

download::DownloadItem* CheckFileSystemAccessWriteRequest::item() const {
  return nullptr;
}

// static
MayCheckDownloadResult CheckFileSystemAccessWriteRequest::IsSupportedDownload(
    const base::FilePath& file_name,
    DownloadCheckResultReason* reason) {
  if (!IsFiletypeSupportedForFullDownloadProtection(file_name)) {
    *reason = REASON_NOT_BINARY_FILE;
    return MayCheckDownloadResult::kMaySendSampledPingOnly;
  }
  return MayCheckDownloadResult::kMayCheckDownload;
}

MayCheckDownloadResult CheckFileSystemAccessWriteRequest::IsSupportedDownload(
    DownloadCheckResultReason* reason) {
  if (!weak_metadata_) {
    *reason = REASON_DOWNLOAD_DESTROYED;
    return MayCheckDownloadResult::kMayNotCheckDownload;
  }
  return IsSupportedDownload(weak_metadata_->GetTargetFilePath(), reason);
}

content::BrowserContext* CheckFileSystemAccessWriteRequest::GetBrowserContext()
    const {
  if (!weak_metadata_) {
    return nullptr;
  }

  return weak_metadata_->GetBrowserContext();
}

bool CheckFileSystemAccessWriteRequest::IsCancelled() {
  return false;
}

base::WeakPtr<CheckClientDownloadRequestBase>
CheckFileSystemAccessWriteRequest::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CheckFileSystemAccessWriteRequest::NotifySendRequest(
    const ClientDownloadRequest* request) {
  service()->file_system_access_write_request_callbacks_.Notify(request);
  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.NativeFileSystemWriteAttribution",
      request->referrer_chain().size());
}

void CheckFileSystemAccessWriteRequest::SetDownloadProtectionData(
    const std::string& token,
    const ClientDownloadResponse::Verdict& verdict,
    const ClientDownloadResponse::TailoredVerdict& tailored_verdict) {
  // TODO(crbug.com/41477698): Actually store token for
  // IncidentReportingService usage.
}

#if !BUILDFLAG(IS_ANDROID)
void CheckFileSystemAccessWriteRequest::MaybeBeginFeedbackForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    const std::string& request_data,
    const std::string& response_body) {
  // TODO(crbug.com/41477698): Integrate with DownloadFeedbackService.
}
#endif

std::optional<enterprise_connectors::AnalysisSettings>
CheckFileSystemAccessWriteRequest::ShouldUploadBinary(
    DownloadCheckResultReason reason) {
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kEnterpriseFileSystemAccessDeepScan)) {
    return std::nullopt;
  }

  // If the download is already considered dangerous, don't upload the binary.
  if (reason == REASON_DOWNLOAD_DANGEROUS ||
      reason == REASON_DOWNLOAD_DANGEROUS_HOST ||
      reason == REASON_DOWNLOAD_DANGEROUS_ACCOUNT_COMPROMISE) {
    return std::nullopt;
  }

  if (!weak_metadata_) {
    return std::nullopt;
  }

  auto settings = DeepScanningRequest::ShouldUploadBinary(*weak_metadata_);

  // Malware scanning is redundant if the URL is allowlisted, but DLP scanning
  // might still need to happen.
  if (settings && reason == REASON_ALLOWLISTED_URL) {
    settings->tags.erase("malware");
    if (settings->tags.empty()) {
      return std::nullopt;
    }
  }
  return settings;
#else
  return std::nullopt;
#endif
}

void CheckFileSystemAccessWriteRequest::UploadBinary(
    DownloadCheckResult result,
    DownloadCheckResultReason reason,
    enterprise_connectors::AnalysisSettings settings) {
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kEnterpriseFileSystemAccessDeepScan)) {
    return;
  }

  // Stores callback in metadata as it's not repeating, and we ensure this
  // callback is only run once.
  metadata_->SetCallback(TakeCallback());

  // Ownership of metadata moved as `CheckFileSystemAccessWriteRequest` will be
  // destroyed before the deep scan finishes.
  service()->UploadForDeepScanning(
      std::move(metadata_),
      base::BindRepeating(&FileSystemAccessMetadata::ProcessScanResult,
                          weak_metadata_, reason),
      DownloadItemWarningData::DeepScanTrigger::TRIGGER_POLICY, result,
      std::move(settings),
      /*password=*/std::nullopt);
#endif
}

bool CheckFileSystemAccessWriteRequest::ShouldImmediatelyDeepScan(
    bool server_requests_prompt) const {
  return false;
}

bool CheckFileSystemAccessWriteRequest::ShouldPromptForDeepScanning(
    bool server_requests_prompt) const {
  return false;
}

bool CheckFileSystemAccessWriteRequest::ShouldPromptForLocalDecryption(
    bool server_requests_prompt) const {
  return false;
}

bool CheckFileSystemAccessWriteRequest::ShouldPromptForIncorrectPassword()
    const {
  return false;
}

bool CheckFileSystemAccessWriteRequest::ShouldShowScanFailure() const {
  return false;
}

void CheckFileSystemAccessWriteRequest::NotifyRequestFinished(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weakptr_factory_.InvalidateWeakPtrs();
}

bool CheckFileSystemAccessWriteRequest::IsAllowlistedByPolicy() const {
  if (!weak_metadata_) {
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(weak_metadata_->GetBrowserContext());
  if (!profile) {
    return false;
  }
  return IsURLAllowlistedByPolicy(weak_metadata_->GetURL(),
                                  *profile->GetPrefs());
}

void CheckFileSystemAccessWriteRequest::LogDeepScanningPrompt(
    bool did_prompt) const {
  NOTREACHED();
}

}  // namespace safe_browsing

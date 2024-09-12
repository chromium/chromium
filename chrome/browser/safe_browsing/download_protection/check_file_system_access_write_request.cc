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
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
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
              service,
              *item)),
      item_(std::move(item)),
      referrer_chain_data_(
          IdentifyReferrerChain(*item_,
                                DownloadProtectionService::
                                    GetDownloadAttributionUserGestureLimit())) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CheckFileSystemAccessWriteRequest ::~CheckFileSystemAccessWriteRequest() =
    default;

download::DownloadItem* CheckFileSystemAccessWriteRequest::item() const {
  return nullptr;
}

bool CheckFileSystemAccessWriteRequest::IsSupportedDownload(
    DownloadCheckResultReason* reason) {
  if (!FileTypePolicies::GetInstance()->IsCheckedBinaryFile(
          item_->target_file_path)) {
    *reason = REASON_NOT_BINARY_FILE;
    return false;
  }
  return true;
}

content::BrowserContext* CheckFileSystemAccessWriteRequest::GetBrowserContext()
    const {
  return item_->browser_context;
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

void CheckFileSystemAccessWriteRequest::MaybeBeginFeedbackForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    const std::string& request_data,
    const std::string& response_body) {
  // TODO(crbug.com/41477698): Integrate with DownloadFeedbackService.
}

std::optional<enterprise_connectors::AnalysisSettings>
CheckFileSystemAccessWriteRequest::ShouldUploadBinary(
    DownloadCheckResultReason reason) {
  return std::nullopt;
}

void CheckFileSystemAccessWriteRequest::UploadBinary(
    DownloadCheckResult result,
    DownloadCheckResultReason reason,
    enterprise_connectors::AnalysisSettings settings) {}

bool CheckFileSystemAccessWriteRequest::ShouldImmediatelyDeepScan(
    bool server_requests_prompt,
    bool log_metrics) const {
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
  Profile* profile = Profile::FromBrowserContext(item_->browser_context);
  if (!profile)
    return false;
  return IsURLAllowlistedByPolicy(item_->frame_url, *profile->GetPrefs());
}

void CheckFileSystemAccessWriteRequest::LogDeepScanningPrompt(
    bool did_prompt) const {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace safe_browsing

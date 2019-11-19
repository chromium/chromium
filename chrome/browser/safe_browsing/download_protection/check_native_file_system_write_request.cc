// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_native_file_system_write_request.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/navigation_entry.h"

namespace safe_browsing {

using content::BrowserThread;

namespace {

GURL GetDownloadUrl(const GURL& frame_url) {
  // Regular blob: URLs are of the form
  // "blob:https://my-origin.com/def07373-cbd8-49d2-9ef7-20b071d34a1a". To make
  // these URLs distinguishable from those we use a fixed string rather than a
  // random UUID.
  return GURL("blob:" + frame_url.GetOrigin().spec() +
              "native-file-system-write");
}

CheckClientDownloadRequestBase::TabUrls TabUrlsFromWebContents(
    content::WebContents* web_contents) {
  CheckClientDownloadRequestBase::TabUrls result;
  if (web_contents) {
    content::NavigationEntry* entry =
        web_contents->GetController().GetVisibleEntry();
    if (entry) {
      result.url = entry->GetURL();
      result.referrer = entry->GetReferrer().url;
    }
  }
  return result;
}

}  // namespace

CheckNativeFileSystemWriteRequest::CheckNativeFileSystemWriteRequest(
    std::unique_ptr<content::NativeFileSystemWriteItem> item,
    CheckDownloadCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
    : CheckClientDownloadRequestBase(GetDownloadUrl(item->frame_url),
                                     item->target_file_path,
                                     item->full_path,
                                     TabUrlsFromWebContents(item->web_contents),
                                     item->size,
                                     item->browser_context,
                                     std::move(callback),
                                     service,
                                     std::move(database_manager),
                                     std::move(binary_feature_extractor)),
      item_(std::move(item)),
      referrer_chain_data_(service->IdentifyReferrerChain(*item_)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CheckNativeFileSystemWriteRequest ::~CheckNativeFileSystemWriteRequest() =
    default;

bool CheckNativeFileSystemWriteRequest::IsSupportedDownload(
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
  if (!FileTypePolicies::GetInstance()->IsCheckedBinaryFile(
          item_->target_file_path)) {
    *reason = REASON_NOT_BINARY_FILE;
    return false;
  }
  *type = download_type_util::GetDownloadType(item_->target_file_path);
  return true;
}

content::BrowserContext*
CheckNativeFileSystemWriteRequest::GetBrowserContext() {
  return item_->browser_context;
}

bool CheckNativeFileSystemWriteRequest::IsCancelled() {
  return false;
}

void CheckNativeFileSystemWriteRequest::PopulateRequest(
    ClientDownloadRequest* request) {
  request->mutable_digests()->set_sha256(item_->sha256_hash);
  request->set_length(item_->size);
  {
    ClientDownloadRequest::Resource* resource = request->add_resources();
    resource->set_url(SanitizeUrl(GetDownloadUrl(item_->frame_url)));
    resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
    if (item_->frame_url.is_valid())
      resource->set_referrer(SanitizeUrl(item_->frame_url));
  }

  request->set_user_initiated(item_->has_user_gesture);

  if (referrer_chain_data_ &&
      !referrer_chain_data_->GetReferrerChain()->empty()) {
    request->mutable_referrer_chain()->Swap(
        referrer_chain_data_->GetReferrerChain());
    request->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data_->recent_navigations_to_collect());
    UMA_HISTOGRAM_COUNTS_100(
        "SafeBrowsing.ReferrerURLChainSize.NativeFileSystemWriteAttribution",
        referrer_chain_data_->referrer_chain_length());
  }
}

base::WeakPtr<CheckClientDownloadRequestBase>
CheckNativeFileSystemWriteRequest::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CheckNativeFileSystemWriteRequest::NotifySendRequest(
    const ClientDownloadRequest* request) {
  service()->native_file_system_write_request_callbacks_.Notify(request);
}

void CheckNativeFileSystemWriteRequest::SetDownloadPingToken(
    const std::string& token) {
  // TODO(https://crbug.com/996797): Actually store token for
  // IncidentReportingService usage.
}

void CheckNativeFileSystemWriteRequest::MaybeStorePingsForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    const std::string& request_data,
    const std::string& response_body) {
  // TODO(https://crbug.com/996797): Integrate with DownloadFeedbackService.
}

bool CheckNativeFileSystemWriteRequest::MaybeReturnAsynchronousVerdict(
    DownloadCheckResultReason reason) {
  return false;
}

bool CheckNativeFileSystemWriteRequest::ShouldUploadBinary(
    DownloadCheckResultReason reason) {
  return false;
}

void CheckNativeFileSystemWriteRequest::UploadBinary(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {}

void CheckNativeFileSystemWriteRequest::NotifyRequestFinished(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weakptr_factory_.InvalidateWeakPtrs();
}

}  // namespace safe_browsing

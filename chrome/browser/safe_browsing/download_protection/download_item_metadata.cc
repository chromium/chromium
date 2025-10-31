// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_metadata.h"

#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "content/public/browser/download_item_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#endif

namespace safe_browsing {

DownloadItemMetadata::DownloadItemMetadata(download::DownloadItem* item)
    : item_(item) {
  CHECK(item_);
}

DownloadItemMetadata::~DownloadItemMetadata() {
  // Stop all active observations.
  for (auto& [observer, observation] : download_observations_) {
    if (observation) {
      observation->Stop();
    }
  }
}

content::BrowserContext* DownloadItemMetadata::GetBrowserContext() const {
  return content::DownloadItemUtils::GetBrowserContext(item_);
}

safe_browsing::ReferrerChain DownloadItemMetadata::GetReferrerChain() const {
  return GetOrIdentifyReferrerChainForEnterprise(*item_);
}

const base::FilePath& DownloadItemMetadata::GetFullPath() const {
  return item_->GetFullPath();
}

const base::FilePath& DownloadItemMetadata::GetTargetFilePath() const {
  return item_->GetTargetFilePath();
}

const std::string& DownloadItemMetadata::GetHash() const {
  return item_->GetHash();
}

int64_t DownloadItemMetadata::GetTotalBytes() const {
  return item_->GetTotalBytes();
}

std::string DownloadItemMetadata::GetMimeType() const {
  return item_->GetMimeType();
}

const GURL& DownloadItemMetadata::GetURL() const {
  return item_->GetURL();
}

const GURL& DownloadItemMetadata::GetTabUrl() const {
  return item_->GetTabUrl();
}

bool DownloadItemMetadata::HasUserGesture() const {
  return item_->HasUserGesture();
}

bool DownloadItemMetadata::IsObfuscated() const {
  enterprise_obfuscation::DownloadObfuscationData* obfuscation_data =
      static_cast<enterprise_obfuscation::DownloadObfuscationData*>(
          item_->GetUserData(
              enterprise_obfuscation::DownloadObfuscationData::kUserDataKey));
  return obfuscation_data ? obfuscation_data->is_obfuscated : false;
}

bool DownloadItemMetadata::IsTopLevelEncryptedArchive() const {
  return DownloadItemWarningData::IsTopLevelEncryptedArchive(item_);
}

download::DownloadDangerType DownloadItemMetadata::GetDangerType() const {
  return item_->GetDangerType();
}

enterprise_connectors::EventResult DownloadItemMetadata::GetPreScanEventResult(
    download::DownloadDangerType danger_type) const {
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(GetBrowserContext());
  if (download_core_service) {
    ChromeDownloadManagerDelegate* delegate =
        download_core_service->GetDownloadManagerDelegate();
    if (delegate && delegate->ShouldBlockFile(item_, danger_type)) {
      return enterprise_connectors::EventResult::BLOCKED;
    }
  }

  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      return enterprise_connectors::EventResult::WARNED;

    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
      return enterprise_connectors::EventResult::ALLOWED;

    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return enterprise_connectors::EventResult::BYPASSED;

    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
    case download::DOWNLOAD_DANGER_TYPE_FORCE_SAVE_TO_GDRIVE:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED();
  }
}

std::unique_ptr<DownloadRequestMaker>
DownloadItemMetadata::CreateDownloadRequestFromMetadata(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor) const {
  return DownloadRequestMaker::CreateFromDownloadItem(binary_feature_extractor,
                                                      item_);
}

std::unique_ptr<DeepScanningMetadata::DownloadScopedObservation>
DownloadItemMetadata::GetDownloadObservation(
    download::DownloadItem::Observer* observer) {
  auto download_observation =
      std::make_unique<DownloadScopedObservation>(this, observer);

  if (item_) {
    download_observation->Observe(item_);
  }

  // Stores a raw pointer to the observation for cleanup.
  download_observations_[observer] =
      raw_ptr<DownloadScopedObservation>(download_observation.get());

  return download_observation;
}

void DownloadItemMetadata::RemoveObservation(
    download::DownloadItem::Observer* observer) {
  download_observations_.erase(observer);
}

void DownloadItemMetadata::SetDeepScanTrigger(
    DownloadItemWarningData::DeepScanTrigger trigger) const {
  DownloadItemWarningData::SetDeepScanTrigger(item_, trigger);
}

void DownloadItemMetadata::SetHasIncorrectPassword(
    bool has_incorrect_password) const {
  DownloadItemWarningData::SetHasIncorrectPassword(item_,
                                                   has_incorrect_password);
}

void DownloadItemMetadata::OpenDownload() const {
  item_->OpenDownload();
}

void DownloadItemMetadata::PromptForPassword() const {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  if (DownloadBubbleUIController* controller =
          DownloadBubbleUIController::GetForDownload(item_);
      controller) {
    controller->GetDownloadDisplayController()->OpenSecuritySubpage(
        OfflineItemUtils::GetContentIdForDownload(item_));
  }
#endif
}

void DownloadItemMetadata::AddScanResultMetadata(
    const enterprise_connectors::FileMetadata& file_metadata) const {
  enterprise_connectors::ScanResult* stored_result =
      static_cast<enterprise_connectors::ScanResult*>(
          item_->GetUserData(enterprise_connectors::ScanResult::kKey));
  if (stored_result) {
    stored_result->file_metadata.push_back(file_metadata);
  } else {
    auto scan_result =
        std::make_unique<enterprise_connectors::ScanResult>(file_metadata);
    item_->SetUserData(enterprise_connectors::ScanResult::kKey,
                       std::move(scan_result));
  }
}

bool DownloadItemMetadata::IsForDownloadItem(
    download::DownloadItem* download) const {
  return item_ == download;
}

void DownloadItemMetadata::SetCallback(
    CheckDownloadRepeatingCallback callback) {
  callback_ = std::move(callback);
}

void DownloadItemMetadata::ProcessScanResult(
    DownloadCheckResultReason reason,
    DownloadCheckResult deep_scan_result) {
  if (!callback_.is_null()) {
    callback_.Run(MaybeOverrideScanResult(reason, deep_scan_result));
  }
}

google::protobuf::RepeatedPtrField<std::string>
DownloadItemMetadata::CollectFrameUrls() const {
  return enterprise_connectors::CollectFrameUrls(
      content::DownloadItemUtils::GetWebContents(item_),
      enterprise_connectors::DeepScanAccessPoint::DOWNLOAD);
}

content::WebContents* DownloadItemMetadata::web_contents() const {
  return content::DownloadItemUtils::GetOriginalWebContents(item_.get());
}

base::WeakPtr<DownloadItemMetadata> DownloadItemMetadata::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

}  // namespace safe_browsing

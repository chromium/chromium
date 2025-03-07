// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_metadata.h"

#include "components/download/public/common/download_item.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

DownloadItemMetadata::DownloadItemMetadata(download::DownloadItem* item)
    : item_(item) {
  CHECK(item_);
}

content::BrowserContext* DownloadItemMetadata::GetBrowserContext() const {
  return content::DownloadItemUtils::GetBrowserContext(item_);
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

std::unique_ptr<DownloadRequestMaker>
DownloadItemMetadata::CreateDownloadRequestFromMetadata(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor) const {
  return DownloadRequestMaker::CreateFromDownloadItem(binary_feature_extractor,
                                                      item_);
}

void DownloadItemMetadata::AddObserver(
    download::DownloadItem::Observer* observer) const {
  item_->AddObserver(observer);
}

void DownloadItemMetadata::RemoveObserver(
    download::DownloadItem::Observer* observer) const {
  item_->RemoveObserver(observer);
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

}  // namespace safe_browsing

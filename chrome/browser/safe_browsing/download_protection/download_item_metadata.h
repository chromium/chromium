// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_METADATA_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_METADATA_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_metadata.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// Implementation of DeepScanningMetadata for DownloadItem.
class DownloadItemMetadata : public DeepScanningMetadata {
 public:
  explicit DownloadItemMetadata(download::DownloadItem* item);

  content::BrowserContext* GetBrowserContext() const override;
  const base::FilePath& GetFullPath() const override;
  const base::FilePath& GetTargetFilePath() const override;
  const std::string& GetHash() const override;
  int64_t GetTotalBytes() const override;
  std::string GetMimeType() const override;
  const GURL& GetURL() const override;
  const GURL& GetTabUrl() const override;
  bool HasUserGesture() const override;
  bool IsObfuscated() const override;
  bool IsTopLevelEncryptedArchive() const override;
  download::DownloadDangerType GetDangerType() const override;

  std::unique_ptr<DownloadRequestMaker> CreateDownloadRequestFromMetadata(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
      const override;

  void AddObserver(download::DownloadItem::Observer* observer) const override;
  void RemoveObserver(
      download::DownloadItem::Observer* observer) const override;
  void SetDeepScanTrigger(
      DownloadItemWarningData::DeepScanTrigger trigger) const override;
  void SetHasIncorrectPassword(bool has_incorrect_password) const override;
  void OpenDownload() const override;

 private:
  raw_ptr<download::DownloadItem> item_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_METADATA_H_

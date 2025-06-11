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
  ~DownloadItemMetadata() override;

  content::BrowserContext* GetBrowserContext() const override;
  safe_browsing::ReferrerChain GetReferrerChain() const override;
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
  enterprise_connectors::EventResult GetPreScanEventResult(
      download::DownloadDangerType danger_type) const override;

  std::unique_ptr<DownloadRequestMaker> CreateDownloadRequestFromMetadata(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
      const override;

  std::unique_ptr<DownloadScopedObservation> GetDownloadObservation(
      download::DownloadItem::Observer* observer) override;
  void RemoveObservation(download::DownloadItem::Observer* observer) override;
  void SetDeepScanTrigger(
      DownloadItemWarningData::DeepScanTrigger trigger) const override;
  void SetHasIncorrectPassword(bool has_incorrect_password) const override;
  void OpenDownload() const override;
  void PromptForPassword() const override;
  void AddScanResultMetadata(
      const enterprise_connectors::FileMetadata& file_metadata) const override;
  bool IsForDownloadItem(download::DownloadItem* download) const override;
  void SetCallback(CheckDownloadRepeatingCallback callback);
  void ProcessScanResult(DownloadCheckResultReason reason,
                         DownloadCheckResult deep_scan_result) override;
  base::WeakPtr<DownloadItemMetadata> GetWeakPtr();

 private:
  // The download item to scan.
  raw_ptr<download::DownloadItem> item_;
  CheckDownloadRepeatingCallback callback_;

  // Map of observers to their observation objects for cleanup.
  std::unordered_map<download::DownloadItem::Observer*,
                     raw_ptr<DownloadScopedObservation>>
      download_observations_;

  base::WeakPtrFactory<DownloadItemMetadata> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_METADATA_H_

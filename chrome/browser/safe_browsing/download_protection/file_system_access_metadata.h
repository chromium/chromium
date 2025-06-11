// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_SYSTEM_ACCESS_METADATA_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_SYSTEM_ACCESS_METADATA_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_metadata.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "content/public/browser/file_system_access_write_item.h"

namespace safe_browsing {

// Implementation of DeepScanningMetadata for FileSystemAccessWriteItem.
class FileSystemAccessMetadata : public DeepScanningMetadata {
 public:
  explicit FileSystemAccessMetadata(
      std::unique_ptr<content::FileSystemAccessWriteItem> item);
  ~FileSystemAccessMetadata() override;

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
  bool IsForDownloadItem(download::DownloadItem* download) const override;
  download::DownloadDangerType GetDangerType() const override;
  enterprise_connectors::EventResult GetPreScanEventResult(
      download::DownloadDangerType danger_type) const override;

  std::unique_ptr<DownloadRequestMaker> CreateDownloadRequestFromMetadata(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
      const override;

  std::unique_ptr<DownloadScopedObservation> GetDownloadObservation(
      download::DownloadItem::Observer* observer) override;

  void SetCallback(CheckDownloadCallback callback);
  void ProcessScanResult(DownloadCheckResultReason reason,
                         DownloadCheckResult deep_scan_result) override;
  base::WeakPtr<FileSystemAccessMetadata> GetWeakPtr();

 private:
  std::unique_ptr<content::FileSystemAccessWriteItem> item_;
  CheckDownloadCallback callback_;

  // Cache computed mime type.
  mutable std::string mime_type_;
  mutable bool mime_type_computed_ = false;

  const GURL tab_url_;

  base::WeakPtrFactory<FileSystemAccessMetadata> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_SYSTEM_ACCESS_METADATA_H_

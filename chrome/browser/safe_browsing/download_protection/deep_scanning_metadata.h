// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_METADATA_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_METADATA_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace safe_browsing {

// Download metadata interface with the subset of methods needed for safe
// browsing deep scanning operations.
class DeepScanningMetadata {
 public:
  virtual ~DeepScanningMetadata() = default;

  // File metadata accessor methods used in deep scanning.
  virtual content::BrowserContext* GetBrowserContext() const = 0;
  virtual const base::FilePath& GetFullPath() const = 0;
  virtual const base::FilePath& GetTargetFilePath() const = 0;
  virtual const std::string& GetHash() const = 0;
  virtual int64_t GetTotalBytes() const = 0;
  virtual std::string GetMimeType() const = 0;
  virtual const GURL& GetURL() const = 0;
  virtual const GURL& GetTabUrl() const = 0;
  virtual bool HasUserGesture() const = 0;
  virtual bool IsObfuscated() const = 0;
  virtual bool IsTopLevelEncryptedArchive() const = 0;

  // Returns danger type before deep scanning begins, and used as a fallback
  // value if deep scanning fails or is interrupted.
  // For `DownloadItem`, danger type is updated with `DownloadCheckResult` when
  // content checks are completed.
  virtual download::DownloadDangerType GetDangerType() const = 0;

  // Populates download request fields from download metadata.
  virtual std::unique_ptr<DownloadRequestMaker>
  CreateDownloadRequestFromMetadata(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor) const = 0;

  // Methods currently only relevant to deep scan requests on `DownloadItem`.
  virtual void AddObserver(download::DownloadItem::Observer* observer) const {}
  virtual void RemoveObserver(
      download::DownloadItem::Observer* observer) const {}
  virtual void SetDeepScanTrigger(
      DownloadItemWarningData::DeepScanTrigger trigger) const {}
  virtual void SetHasIncorrectPassword(bool has_incorrect_password) const {}
  virtual void OpenDownload() const {}
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_METADATA_H_

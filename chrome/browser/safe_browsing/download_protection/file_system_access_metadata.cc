// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/file_system_access_metadata.h"

#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "net/base/mime_util.h"

namespace safe_browsing {

FileSystemAccessMetadata::FileSystemAccessMetadata(
    std::unique_ptr<content::FileSystemAccessWriteItem> item)
    : item_(std::move(item)),
      tab_url_(item_->web_contents ? item_->web_contents->GetLastCommittedURL()
                                   : GURL()) {
  CHECK(item_);
}

FileSystemAccessMetadata::~FileSystemAccessMetadata() = default;

content::BrowserContext* FileSystemAccessMetadata::GetBrowserContext() const {
  return item_->browser_context;
}

safe_browsing::ReferrerChain FileSystemAccessMetadata::GetReferrerChain()
    const {
  std::unique_ptr<safe_browsing::ReferrerChainData> referrer_chain_data =
      safe_browsing::IdentifyReferrerChain(
          *item_, enterprise_connectors::kReferrerUserGestureLimit);
  if (referrer_chain_data && referrer_chain_data->GetReferrerChain()) {
    return *referrer_chain_data->GetReferrerChain();
  }
  return safe_browsing::ReferrerChain();
}

const base::FilePath& FileSystemAccessMetadata::GetFullPath() const {
  return item_->full_path;
}

const base::FilePath& FileSystemAccessMetadata::GetTargetFilePath() const {
  return item_->target_file_path;
}

const std::string& FileSystemAccessMetadata::GetHash() const {
  return item_->sha256_hash;
}

int64_t FileSystemAccessMetadata::GetTotalBytes() const {
  return item_->size;
}

std::string FileSystemAccessMetadata::GetMimeType() const {
  if (!mime_type_computed_) {
    // Get MIME type based on file extension.
    std::string mime_type;
    base::FilePath::StringType ext =
        base::ToLowerASCII(item_->target_file_path.FinalExtension());

    // Remove leading dot from extension.
    if (!ext.empty() && ext[0] == FILE_PATH_LITERAL('.')) {
      ext = ext.substr(1);
    }

    // Searches within chrome's built-in list of filetype/extension
    // associations, as using `GetMimeTypeFromExtension` includes
    // platform-defined mime type mappings that can potentially block.
    // TODO(crbug.com/407598185): Add platform MIME types lookup for better
    // protection.
    if (net::GetWellKnownMimeTypeFromExtension(ext, &mime_type)) {
      mime_type_ = mime_type;
    } else {
      // Default to octet-stream for unknown MIME type.
      mime_type_ = "application/octet-stream";
    }
    mime_type_computed_ = true;
  }
  return mime_type_;
}

const GURL& FileSystemAccessMetadata::GetURL() const {
  return item_->frame_url;
}

const GURL& FileSystemAccessMetadata::GetTabUrl() const {
  return tab_url_;
}

bool FileSystemAccessMetadata::HasUserGesture() const {
  return item_->has_user_gesture;
}

bool FileSystemAccessMetadata::IsObfuscated() const {
  // No support for enterprise obfuscation for filesystem access API.
  return false;
}

bool FileSystemAccessMetadata::IsTopLevelEncryptedArchive() const {
  // No support for password-protected files for filesystem access API.
  return false;
}

bool FileSystemAccessMetadata::IsForDownloadItem(
    download::DownloadItem* download) const {
  return false;
}

download::DownloadDangerType FileSystemAccessMetadata::GetDangerType() const {
  // Used as the default pre-scan and fallback danger type since FSA doesn't
  // have preliminary danger type checks.
  return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
}

enterprise_connectors::EventResult
FileSystemAccessMetadata::GetPreScanEventResult(
    download::DownloadDangerType danger_type) const {
  // Currently for file system access deep scans, pre-scan danger type
  // should always be DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS and event result
  // ALLOWED.
  if (danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    DVLOG(1) << "Unexpected danger type for fsa scan: " << danger_type;
  }
  return enterprise_connectors::EventResult::ALLOWED;
}

std::unique_ptr<DownloadRequestMaker>
FileSystemAccessMetadata::CreateDownloadRequestFromMetadata(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor) const {
  return DownloadRequestMaker::CreateFromFileSystemAccess(
      binary_feature_extractor, *item_);
}

std::unique_ptr<DeepScanningMetadata::DownloadScopedObservation>
FileSystemAccessMetadata::GetDownloadObservation(
    download::DownloadItem::Observer* observer) {
  return nullptr;
}

void FileSystemAccessMetadata::SetCallback(CheckDownloadCallback callback) {
  callback_ = std::move(callback);
}

void FileSystemAccessMetadata::ProcessScanResult(
    DownloadCheckResultReason reason,
    DownloadCheckResult deep_scan_result) {
  if (deep_scan_result == DownloadCheckResult::ASYNC_SCANNING ||
      deep_scan_result == DownloadCheckResult::ASYNC_LOCAL_PASSWORD_SCANNING) {
    return;
  }

  // Callback should only be run once, ignore any subsequent scan results.
  if (!callback_.is_null()) {
    std::move(callback_).Run(MaybeOverrideScanResult(reason, deep_scan_result));
  }
}

google::protobuf::RepeatedPtrField<std::string>
FileSystemAccessMetadata::CollectFrameUrls() const {
  return enterprise_connectors::CollectFrameUrls(
      item_->web_contents.get(),
      enterprise_connectors::DeepScanAccessPoint::DOWNLOAD);
}

content::WebContents* FileSystemAccessMetadata::web_contents() const {
  return item_->web_contents.get();
}

base::WeakPtr<FileSystemAccessMetadata> FileSystemAccessMetadata::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

}  // namespace safe_browsing

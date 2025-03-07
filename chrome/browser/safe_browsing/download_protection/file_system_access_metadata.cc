// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/file_system_access_metadata.h"

#include "components/download/public/common/download_danger_type.h"
#include "net/base/mime_util.h"

namespace safe_browsing {

FileSystemAccessMetadata::FileSystemAccessMetadata(
    std::unique_ptr<content::FileSystemAccessWriteItem> item)
    : item_(std::move(item)) {
  CHECK(item_);
}

FileSystemAccessMetadata::~FileSystemAccessMetadata() = default;

content::BrowserContext* FileSystemAccessMetadata::GetBrowserContext() const {
  return item_->browser_context;
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

    if (net::GetMimeTypeFromExtension(ext, &mime_type)) {
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
  return item_->web_contents->GetLastCommittedURL();
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

download::DownloadDangerType FileSystemAccessMetadata::GetDangerType() const {
  // Used as the default pre-scan and fallback danger type since FSA doesn't
  // have preliminary danger type checks.
  return download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
}

std::unique_ptr<DownloadRequestMaker>
FileSystemAccessMetadata::CreateDownloadRequestFromMetadata(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor) const {
  return DownloadRequestMaker::CreateFromFileSystemAccess(
      binary_feature_extractor, *item_);
}

}  // namespace safe_browsing

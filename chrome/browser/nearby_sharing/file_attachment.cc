// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/file_attachment.h"

#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "net/base/mime_util.h"

namespace {

FileAttachment::Type FileAttachmentTypeFromMimeType(
    const std::string& mime_type) {
  if (base::StartsWith(mime_type, "image/"))
    return FileAttachment::Type::kImage;

  if (base::StartsWith(mime_type, "video/"))
    return FileAttachment::Type::kVideo;

  if (base::StartsWith(mime_type, "audio/"))
    return FileAttachment::Type::kAudio;

  return FileAttachment::Type::kUnknown;
}

std::string MimeTypeFromPath(const base::FilePath& path) {
  std::string mime_type = "application/octet-stream";
  base::FilePath::StringType ext = path.Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);

  return mime_type;
}

}  // namespace

FileAttachment::FileAttachment(const base::FilePath& file_path)
    : FileAttachment(file_path, file_path.BaseName()) {}

FileAttachment::FileAttachment(const base::FilePath& file_path,
                               const base::FilePath& base_name)
    : Attachment(Attachment::Family::kFile, /*size=*/0),
      file_name_(base_name.AsUTF8Unsafe()),
      mime_type_(MimeTypeFromPath(base_name)),
      type_(FileAttachmentTypeFromMimeType(mime_type_)),
      file_path_(file_path) {}

FileAttachment::FileAttachment(int64_t id,
                               int64_t size,
                               std::string file_name,
                               std::string mime_type,
                               Type type)
    : Attachment(id, Attachment::Family::kFile, size),
      file_name_(std::move(file_name)),
      mime_type_(std::move(mime_type)),
      type_(type) {}

FileAttachment::FileAttachment(const FileAttachment&) = default;

FileAttachment::FileAttachment(FileAttachment&&) = default;

FileAttachment& FileAttachment::operator=(const FileAttachment&) = default;

FileAttachment& FileAttachment::operator=(FileAttachment&&) = default;

FileAttachment::~FileAttachment() = default;

void FileAttachment::MoveToShareTarget(ShareTarget& share_target) {
  share_target.file_attachments.push_back(std::move(*this));
}

const std::string& FileAttachment::GetDescription() const {
  return file_name_;
}

nearby_share::mojom::ShareType FileAttachment::GetShareType() const {
  switch (type()) {
    case FileAttachment::Type::kImage:
      return nearby_share::mojom::ShareType::kImageFile;
    case FileAttachment::Type::kVideo:
      return nearby_share::mojom::ShareType::kVideoFile;
    case FileAttachment::Type::kAudio:
      return nearby_share::mojom::ShareType::kAudioFile;
    default:
      break;
  }

  // Try matching on mime type if the attachment type is unrecognized.
  if (mime_type() == "application/pdf") {
    return nearby_share::mojom::ShareType::kPdfFile;
  } else if (mime_type() == "application/vnd.google-apps.document") {
    return nearby_share::mojom::ShareType::kGoogleDocsFile;
  } else if (mime_type() == "application/vnd.google-apps.spreadsheet") {
    return nearby_share::mojom::ShareType::kGoogleSheetsFile;
  } else if (mime_type() == "application/vnd.google-apps.presentation") {
    return nearby_share::mojom::ShareType::kGoogleSlidesFile;
  } else {
    return nearby_share::mojom::ShareType::kUnknownFile;
  }
}

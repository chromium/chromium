// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_media_parser.h"

#include <string_view>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/media_galleries/blob_data_source_factory.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_media_parser_util.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blob_reader.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"

namespace extensions {

FileManagerPrivateInternalGetContentMimeTypeFunction::
    FileManagerPrivateInternalGetContentMimeTypeFunction() = default;

FileManagerPrivateInternalGetContentMimeTypeFunction::
    ~FileManagerPrivateInternalGetContentMimeTypeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetContentMimeTypeFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& blob_uuid = args()[0].GetString();

  if (blob_uuid.empty()) {
    return RespondNow(Error("Blob UUID must not be empty."));
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FileManagerPrivateInternalGetContentMimeTypeFunction::ReadBlobBytes,
          this, blob_uuid));

  return RespondLater();
}

void FileManagerPrivateInternalGetContentMimeTypeFunction::ReadBlobBytes(
    const std::string& blob_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BlobReader::Read(  // Read net::kMaxBytesToSniff bytes from the front.
      browser_context()->GetBlobRemote(blob_uuid),
      base::BindOnce(
          &FileManagerPrivateInternalGetContentMimeTypeFunction::SniffMimeType,
          this, blob_uuid),
      0, net::kMaxBytesToSniff);
}

void FileManagerPrivateInternalGetContentMimeTypeFunction::SniffMimeType(
    const std::string& blob_uuid,
    std::string sniff_bytes,
    int64_t /*length*/) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string mime_type;
  if (!net::SniffMimeTypeFromLocalData(sniff_bytes, &mime_type)) {
    Respond(Error("Could not deduce the content mime type."));
    return;
  }

  Respond(WithArguments(mime_type));
}

FileManagerPrivateInternalGetContentMetadataFunction::
    FileManagerPrivateInternalGetContentMetadataFunction() = default;

FileManagerPrivateInternalGetContentMetadataFunction::
    ~FileManagerPrivateInternalGetContentMetadataFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetContentMetadataFunction::Run() {
  using api::file_manager_private_internal::GetContentMetadata::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->blob_uuid.empty()) {
    return RespondNow(Error("Blob UUID must not be empty"));
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FileManagerPrivateInternalGetContentMetadataFunction::ReadBlobSize,
          this, params->blob_uuid, params->mime_type, params->include_images));

  return RespondLater();
}

void FileManagerPrivateInternalGetContentMetadataFunction::ReadBlobSize(
    const std::string& blob_uuid,
    const std::string& mime_type,
    bool include_images) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BlobReader::Read(  // Read net::kMaxBytesToSniff bytes from the front.
      browser_context()->GetBlobRemote(blob_uuid),
      base::BindOnce(
          &FileManagerPrivateInternalGetContentMetadataFunction::CanParseBlob,
          this, blob_uuid, mime_type, include_images),
      0, net::kMaxBytesToSniff);
}

void FileManagerPrivateInternalGetContentMetadataFunction::CanParseBlob(
    const std::string& blob_uuid,
    const std::string& mime_type,
    bool include_images,
    std::string /*sniff_bytes*/,
    int64_t length) {  // The length of the original input blob.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!net::MatchesMimeType("audio/*", mime_type) &&
      !net::MatchesMimeType("video/*", mime_type)) {
    Respond(Error("An audio or video mime type is required."));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FileManagerPrivateInternalGetContentMetadataFunction::ParseBlob,
          this, blob_uuid, mime_type, include_images, length));
}

void FileManagerPrivateInternalGetContentMetadataFunction::ParseBlob(
    const std::string& blob_uuid,
    const std::string& mime_type,
    bool include_images,
    int64_t length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto input_blob_data_source =
      std::make_unique<chrome_apps::api::BlobDataSourceFactory>(
          browser_context(), blob_uuid);
  auto metadata_parser = std::make_unique<SafeMediaMetadataParser>(
      length, mime_type, include_images, std::move(input_blob_data_source));

  auto* safe_media_metadata_parser = metadata_parser.get();
  safe_media_metadata_parser->Start(base::BindOnce(
      &FileManagerPrivateInternalGetContentMetadataFunction::ParserDone, this,
      std::move(metadata_parser)));
}

void FileManagerPrivateInternalGetContentMetadataFunction::ParserDone(
    std::unique_ptr<SafeMediaMetadataParser> parser_keep_alive,
    bool parser_success,
    chrome::mojom::MediaMetadataPtr metadata,
    std::unique_ptr<std::vector<metadata::AttachedImage>> images) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!parser_success) {
    Respond(Error("Could not parse the media metadata."));
    return;
  }

  DCHECK(metadata);
  base::Value::Dict dictionary =
      extensions::api::file_manager_private::MojoMediaMetadataToValue(
          std::move(metadata));

  metadata::AttachedImage* image = nullptr;
  int size = 0;

  DCHECK(images);
  if (!images->empty()) {
    image = &images->front();
    if (base::IsValueInRangeForNumericType<int>(image->data.size())) {
      size = base::checked_cast<int>(image->data.size());
    }
  }

  if (image && size && !image->type.empty()) {  // Attach thumbnail image.
    std::string url =
        base::Base64Encode(std::string_view(image->data.data(), size));
    url.insert(0, base::StrCat({"data:", image->type, ";base64,"}));

    base::Value::Dict media_thumbnail_image;
    media_thumbnail_image.Set("data", std::move(url));
    media_thumbnail_image.Set("type", std::move(image->type));

    base::Value::List* attached_images_list =
        dictionary.FindList("attachedImages");
    attached_images_list->Append(std::move(media_thumbnail_image));
  }

  Respond(WithArguments(std::move(dictionary)));
}

}  // namespace extensions

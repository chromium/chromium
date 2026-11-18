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
#include "chromeos/ash/experiences/extensions/common/api/file_manager_private.h"
#include "chromeos/ash/experiences/extensions/common/api/file_manager_private_internal.h"
#include "components/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blob_reader.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace extensions {

FileManagerPrivateInternalGetContentMimeTypeFunction::
    FileManagerPrivateInternalGetContentMimeTypeFunction() = default;

FileManagerPrivateInternalGetContentMimeTypeFunction::
    ~FileManagerPrivateInternalGetContentMimeTypeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetContentMimeTypeFunction::Run() {
  using api::file_manager_private_internal::GetContentMimeType::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  mojo::PendingRemote<blink::mojom::Blob> blob = TakeBlobArg(0);
  EXTENSION_FUNCTION_VALIDATE(blob);

  BlobReader::Read(
      std::move(blob),
      base::BindOnce(
          &FileManagerPrivateInternalGetContentMimeTypeFunction::SniffMimeType,
          this),
      0, net::kMaxBytesToSniff);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void FileManagerPrivateInternalGetContentMimeTypeFunction::SniffMimeType(
    std::string sniff_bytes,
    int64_t /*length*/) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);
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

  mojo::PendingRemote<blink::mojom::Blob> blob_arg = TakeBlobArg(0);
  EXTENSION_FUNCTION_VALIDATE(blob_arg);
  mojo::Remote<blink::mojom::Blob> blob(std::move(blob_arg));

  mojo::PendingRemote<blink::mojom::Blob> blob_for_read;
  blob->Clone(blob_for_read.InitWithNewPipeAndPassReceiver());
  BlobReader::Read(  // Read net::kMaxBytesToSniff bytes from the front.
      std::move(blob_for_read),
      base::BindOnce(
          &FileManagerPrivateInternalGetContentMetadataFunction::CanParseBlob,
          this, std::move(blob), params->mime_type, params->include_images),
      0, net::kMaxBytesToSniff);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void FileManagerPrivateInternalGetContentMetadataFunction::CanParseBlob(
    mojo::Remote<blink::mojom::Blob> blob,
    const std::string& mime_type,
    bool include_images,
    std::string /*sniff_bytes*/,
    int64_t length) {  // The length of the original input blob.
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  if (!net::MatchesMimeType("audio/*", mime_type) &&
      !net::MatchesMimeType("video/*", mime_type)) {
    Respond(Error("An audio or video mime type is required."));
    return;
  }

  auto input_blob_data_source =
      std::make_unique<chrome_apps::api::BlobDataSourceFactory>(
          std::move(blob));
  auto metadata_parser = std::make_unique<SafeMediaMetadataParser>(
      length, mime_type, include_images, std::move(input_blob_data_source));

  SafeMediaMetadataParser* safe_media_metadata_parser = metadata_parser.get();
  safe_media_metadata_parser->Start(base::BindOnce(
      &FileManagerPrivateInternalGetContentMetadataFunction::ParserDone, this,
      std::move(metadata_parser)));
}

void FileManagerPrivateInternalGetContentMetadataFunction::ParserDone(
    std::unique_ptr<SafeMediaMetadataParser> parser_keep_alive,
    bool parser_success,
    chrome::mojom::MediaMetadataPtr metadata,
    std::unique_ptr<std::vector<metadata::AttachedImage>> images) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M160);

  if (!parser_success) {
    Respond(Error("Could not parse the media metadata."));
    return;
  }

  CHECK(metadata, base::NotFatalUntil::M160);
  base::DictValue dictionary =
      extensions::api::file_manager_private::MojoMediaMetadataToValue(
          std::move(metadata));

  metadata::AttachedImage* image = nullptr;
  int size = 0;

  CHECK(images, base::NotFatalUntil::M160);
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

    base::DictValue media_thumbnail_image;
    media_thumbnail_image.Set("data", std::move(url));
    media_thumbnail_image.Set("type", std::move(image->type));

    base::ListValue* attached_images_list =
        dictionary.FindList("attachedImages");
    attached_images_list->Append(std::move(media_thumbnail_image));
  }

  Respond(WithArguments(std::move(dictionary)));
}

}  // namespace extensions

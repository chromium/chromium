// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "components/media_gallery_util/public/mojom/media_parser.mojom-forward.h"
#include "components/media_gallery_util/safe_media_metadata_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-forward.h"

namespace extensions {

// Implements the chrome.fileManagerPrivate.getContentMimeType method. Returns
// the content sniffed mime type of a file blob.
class FileManagerPrivateInternalGetContentMimeTypeFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetContentMimeTypeFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getContentMimeType",
                             FILEMANAGERPRIVATEINTERNAL_GETCONTENTMIMETYPE)
 protected:
  ~FileManagerPrivateInternalGetContentMimeTypeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void SniffMimeType(std::string sniff_bytes, int64_t length);
};

// Implements the chrome.fileManagerPrivate.getContentMetadata method. Returns
// metadata tags and images found in audio and video file blobs.
class FileManagerPrivateInternalGetContentMetadataFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetContentMetadataFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getContentMetadata",
                             FILEMANAGERPRIVATEINTERNAL_GETCONTENTMETADATA)
 protected:
  ~FileManagerPrivateInternalGetContentMetadataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void CanParseBlob(  // Only audio and video mime types are supported.
      mojo::Remote<blink::mojom::Blob> blob,
      const std::string& mime_type,
      bool include_images,
      std::string sniff_bytes,
      int64_t length);

  void ParserDone(  // Returns the parsed metadata.
      std::unique_ptr<SafeMediaMetadataParser> parser_keep_alive,
      bool parser_success,
      chrome::mojom::MediaMetadataPtr metadata,
      std::unique_ptr<std::vector<metadata::AttachedImage>> images);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MEDIA_PARSER_H_

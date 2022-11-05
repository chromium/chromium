// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides API functions to fetch external thumbnails for filesystem
// providers that support it.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_THUMBNAIL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_THUMBNAIL_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/services/printing/public/mojom/pdf_thumbnailer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/file_system/file_system_url.h"

class SkBitmap;

namespace extensions {

// Base class for thumbnail functions
class FileManagerPrivateGetThumbnailFunction : public LoggedExtensionFunction {
 public:
  FileManagerPrivateGetThumbnailFunction();

 protected:
  ~FileManagerPrivateGetThumbnailFunction() override = default;

  // Responds with a base64 encoded PNG thumbnail data.
  void SendEncodedThumbnail(std::string thumbnail_data_url);
};

class FileManagerPrivateInternalGetDriveThumbnailFunction
    : public FileManagerPrivateGetThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getDriveThumbnail",
                             FILEMANAGERPRIVATEINTERNAL_GETDRIVETHUMBNAIL)

  FileManagerPrivateInternalGetDriveThumbnailFunction();

 protected:
  ~FileManagerPrivateInternalGetDriveThumbnailFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // A callback invoked when thumbnail data has been generated.
  void GotThumbnail(const absl::optional<std::vector<uint8_t>>& data);
};

class FileManagerPrivateInternalGetPdfThumbnailFunction
    : public FileManagerPrivateGetThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getPdfThumbnail",
                             FILEMANAGERPRIVATEINTERNAL_GETPDFTHUMBNAIL)

  FileManagerPrivateInternalGetPdfThumbnailFunction();

 protected:
  ~FileManagerPrivateInternalGetPdfThumbnailFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // For a given |content| starts fetching the first page PDF thumbnail by
  // calling PdfThumbnailer from the printing service. The first parameters,
  // |size| is supplied by the JavaScript caller.
  void FetchThumbnail(const gfx::Size& size, const std::string& content);

  // Callback invoked by the thumbnailing service when a PDF thumbnail has been
  // generated. The solitary parameter |bitmap| is supplied by the callback.
  // If |bitmap| is null, an error occurred. Otherwise, |bitmap| contains the
  // generated thumbnail.
  void GotThumbnail(const SkBitmap& bitmap);

  // Handles a mojo channel disconnect event.
  void ThumbnailDisconnected();

  // Holds the channel to Printing PDF thumbnailing service. Bound only
  // when needed.
  mojo::Remote<printing::mojom::PdfThumbnailer> pdf_thumbnailer_;

  // The dots per inch (dpi) resolution at which the PDF is rendered to a
  // thumbnail. The value of 30 is selected so that a US Letter size page does
  // not overflow a kSize x kSize thumbnail.
  constexpr static int kDpi = 30;
};

class FileManagerPrivateInternalGetArcDocumentsProviderThumbnailFunction
    : public FileManagerPrivateGetThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.getArcDocumentsProviderThumbnail",
      FILEMANAGERPRIVATEINTERNAL_GETARCDOCUMENTSPROVIDERTHUMBNAIL)

  FileManagerPrivateInternalGetArcDocumentsProviderThumbnailFunction();

 protected:
  ~FileManagerPrivateInternalGetArcDocumentsProviderThumbnailFunction()
      override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // A callback invoked when ExtraFileMetadata is returned from ARC.
  void OnGetExtraFileMetadata(
      const gfx::Size& size_hint,
      const storage::FileSystemURL& file_system_url,
      base::File::Error result,
      const arc::ArcDocumentsProviderRoot::ExtraFileMetadata& metadata);

  // A callback invoked when a FilesystemURL is resolved to content URLs.
  // |paths_to_share| is always expected to be empty because
  // ArcDocumentsProviderThumbnail related functions do not share path
  // to ARCVM via Seneschal.
  void GotContentUrls(const gfx::Size& size_hint,
                      const std::vector<GURL>& urls,
                      const std::vector<base::FilePath>& paths_to_share);

  // A callback invoked when ARC thumbnail file has been opened.
  void GotArcThumbnailFileHandle(mojo::ScopedHandle handle);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_THUMBNAIL_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_thumbnail.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/strcat.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {
namespace {

// Encodes PNG data as a data URL.
std::string MakeThumbnailDataUrlOnThreadPool(
    base::span<const uint8_t> png_data) {
  base::AssertLongCPUWorkAllowed();
  return base::StrCat({"data:image/png;base64,", base::Base64Encode(png_data)});
}

// Converts bitmap to a PNG image and encodes it as a data URL.
std::string ConvertAndEncode(const SkBitmap& bitmap) {
  if (bitmap.isNull()) {
    DLOG(WARNING) << "Got an invalid bitmap";
    return std::string();
  }
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  sk_sp<SkData> png_data(image->encodeToData(SkEncodedImageFormat::kPNG, 100));
  if (!png_data) {
    DLOG(WARNING) << "Thumbnail encoding error";
    return std::string();
  }
  return MakeThumbnailDataUrlOnThreadPool(
      base::make_span(png_data->bytes(), png_data->size()));
}

// The maximum size of the input PDF file for which thumbnails are generated.
constexpr uint32_t kMaxPdfSizeInBytes = 1024u * 1024u;

// A function that performs IO operations to read and render PDF thumbnail
// Must be run by a blocking task runner.
std::string ReadLocalPdf(const base::FilePath& pdf_file_path) {
  int64_t file_size;
  if (!base::GetFileSize(pdf_file_path, &file_size)) {
    DLOG(ERROR) << "Failed to get file size of " << pdf_file_path;
    return std::string();
  }
  if (file_size > kMaxPdfSizeInBytes) {
    DLOG(ERROR) << "File " << pdf_file_path << " is too large " << file_size;
    return std::string();
  }
  std::string contents;
  if (!base::ReadFileToString(pdf_file_path, &contents)) {
    DLOG(ERROR) << "Failed to load " << pdf_file_path;
    return std::string();
  }
  return contents;
}

}  // namespace

FileManagerPrivateGetThumbnailFunction::FileManagerPrivateGetThumbnailFunction()
    : chrome_details_(this) {}

void FileManagerPrivateGetThumbnailFunction::SendEncodedThumbnail(
    std::string thumbnail_data_url) {
  Respond(OneArgument(
      std::make_unique<base::Value>(std::move(thumbnail_data_url))));
}

FileManagerPrivateInternalGetDriveThumbnailFunction::
    FileManagerPrivateInternalGetDriveThumbnailFunction() {
  SetWarningThresholds(base::TimeDelta::FromSeconds(5),
                       base::TimeDelta::FromMinutes(1));
}

FileManagerPrivateInternalGetDriveThumbnailFunction::
    ~FileManagerPrivateInternalGetDriveThumbnailFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDriveThumbnailFunction::Run() {
  using extensions::api::file_manager_private_internal::GetDriveThumbnail::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  if (file_system_url.type() != storage::kFileSystemTypeDriveFs) {
    return RespondNow(Error("Expected a Drivefs URL"));
  }

  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          chrome_details_.GetProfile());
  if (!drive_integration_service) {
    return RespondNow(Error("Drive service not available"));
  }

  base::FilePath path;
  if (!drive_integration_service->GetRelativeDrivePath(file_system_url.path(),
                                                       &path)) {
    return RespondNow(Error("File not found"));
  }

  auto* drivefs_interface = drive_integration_service->GetDriveFsInterface();
  if (!drivefs_interface) {
    return RespondNow(Error("Drivefs not available"));
  }

  drivefs_interface->GetThumbnail(
      path, params->crop_to_square,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&FileManagerPrivateInternalGetDriveThumbnailFunction::
                             GotThumbnail,
                         this),
          base::nullopt));
  return RespondLater();
}

void FileManagerPrivateInternalGetDriveThumbnailFunction::GotThumbnail(
    const base::Optional<std::vector<uint8_t>>& data) {
  if (!data) {
    Respond(OneArgument(std::make_unique<base::Value>("")));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&MakeThumbnailDataUrlOnThreadPool, *data),
      base::BindOnce(&FileManagerPrivateInternalGetDriveThumbnailFunction::
                         SendEncodedThumbnail,
                     this));
}

FileManagerPrivateInternalGetPdfThumbnailFunction::
    FileManagerPrivateInternalGetPdfThumbnailFunction() = default;

FileManagerPrivateInternalGetPdfThumbnailFunction::
    ~FileManagerPrivateInternalGetPdfThumbnailFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetPdfThumbnailFunction::Run() {
  using extensions::api::file_manager_private_internal::GetPdfThumbnail::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  if (file_system_url.type() != storage::kFileSystemTypeNativeLocal) {
    return RespondNow(Error("Expected a native local URL"));
  }

  base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), chrome_details_.GetProfile(), url);
  if (path.empty() ||
      base::FilePath::CompareIgnoreCase(path.Extension(), ".pdf") != 0) {
    return RespondNow(Error("Can only handle PDF files"));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadLocalPdf, std::move(path)),
      base::BindOnce(
          &FileManagerPrivateInternalGetPdfThumbnailFunction::FetchThumbnail,
          this, gfx::Size(params->width, params->height)));
  return RespondLater();
}

void FileManagerPrivateInternalGetPdfThumbnailFunction::FetchThumbnail(
    const gfx::Size& size,
    const std::string& content) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (content.empty()) {
    Respond(Error("Failed to read PDF file"));
    return;
  }
  auto pdf_region = base::ReadOnlySharedMemoryRegion::Create(content.size());
  if (!pdf_region.IsValid()) {
    Respond(Error("Failed allocate memory for PDF file"));
    return;
  }
  memcpy(pdf_region.mapping.memory(), content.data(), content.size());
  DCHECK(!pdf_thumbnailer_.is_bound());
  GetPrintingService()->BindPdfThumbnailer(
      pdf_thumbnailer_.BindNewPipeAndPassReceiver());
  pdf_thumbnailer_.set_disconnect_handler(base::BindOnce(
      &FileManagerPrivateInternalGetPdfThumbnailFunction::ThumbnailDisconnected,
      base::Unretained(this)));
  auto params = printing::mojom::ThumbParams::New(
      /*size_px=*/size, /*dpi=*/gfx::Size(kDpi, kDpi),
      /*stretch_to_bounds=*/false, /*keep_aspect_ratio=*/true);
  pdf_thumbnailer_->GetThumbnail(
      std::move(params), std::move(pdf_region.region),
      base::BindOnce(
          &FileManagerPrivateInternalGetPdfThumbnailFunction::GotThumbnail,
          this));
}

void FileManagerPrivateInternalGetPdfThumbnailFunction::
    ThumbnailDisconnected() {
  DLOG(WARNING) << "PDF thumbnail disconnected";
  Respond(Error("PDF service disconnected"));
}

void FileManagerPrivateInternalGetPdfThumbnailFunction::GotThumbnail(
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pdf_thumbnailer_.reset();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ConvertAndEncode, bitmap),
      base::BindOnce(&FileManagerPrivateInternalGetPdfThumbnailFunction::
                         SendEncodedThumbnail,
                     this));
}

}  // namespace extensions

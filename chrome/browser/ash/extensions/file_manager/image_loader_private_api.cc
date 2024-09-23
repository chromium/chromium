// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/extensions/file_manager/image_loader_private_api.h"

#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/pdf/pdf_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/image_loader_private.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {
namespace {

constexpr char kMimeTypeImagePng[] = "image/png";

// Encodes binary data as a data URL.
std::string MakeThumbnailDataUrlOnThreadPool(const std::string& mimeType,
                                             base::span<const uint8_t> data) {
  base::AssertLongCPUWorkAllowed();
  return base::StrCat(
      {"data:", mimeType, ";base64,", base::Base64Encode(data)});
}

// Converts bitmap to a PNG image and encodes it as a data URL.
std::string ConvertAndEncode(const SkBitmap& bitmap) {
  if (bitmap.isNull()) {
    DLOG(WARNING) << "Got an invalid bitmap";
    return std::string();
  }
  SkDynamicMemoryWStream stream;
  if (!SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}) ||
      !stream.bytesWritten()) {
    DLOG(WARNING) << "Thumbnail encoding error";
    return std::string();
  }
  sk_sp<SkData> png_data = stream.detachAsData();
  return MakeThumbnailDataUrlOnThreadPool(
      kMimeTypeImagePng, base::make_span(png_data->bytes(), png_data->size()));
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

// Max size of a file returned by DocumentsProvider openDocumentThumbnail()
// Arbitrary, but sets some sensible upper limit.
constexpr int kDocumentsProviderMaxThumbnailSizeInBytes = 1024 * 1024;

std::string ReadMojoHandleToDataUrl(mojo::PlatformHandle&& handle) {
  base::ScopedFILE file(FileToFILE(base::File(handle.ReleaseFD()), "rb"));
  std::string contents;
  if (!base::ReadStreamToStringWithMaxSize(
          file.get(), kDocumentsProviderMaxThumbnailSizeInBytes, &contents)) {
    return std::string();
  }
  std::string mime_type;
  if (!net::SniffMimeTypeFromLocalData(contents, &mime_type)) {
    return std::string();
  }
  if (!net::MatchesMimeType("image/*", mime_type)) {
    return std::string();
  }
  return MakeThumbnailDataUrlOnThreadPool(mime_type,
                                          base::as_byte_span(contents));
}

}  // namespace

ImageLoaderPrivateGetThumbnailFunction::
    ImageLoaderPrivateGetThumbnailFunction() = default;

void ImageLoaderPrivateGetThumbnailFunction::SendEncodedThumbnail(
    std::string thumbnail_data_url) {
  Respond(WithArguments(std::move(thumbnail_data_url)));
}

ImageLoaderPrivateGetDriveThumbnailFunction::
    ImageLoaderPrivateGetDriveThumbnailFunction() {
  SetWarningThresholds(base::Seconds(5), base::Minutes(1));
}

ImageLoaderPrivateGetDriveThumbnailFunction::
    ~ImageLoaderPrivateGetDriveThumbnailFunction() = default;

ExtensionFunction::ResponseAction
ImageLoaderPrivateGetDriveThumbnailFunction::Run() {
  using extensions::api::image_loader_private::GetDriveThumbnail::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURLInFirstPartyContext(url);

  if (file_system_url.type() != storage::kFileSystemTypeDriveFs) {
    return RespondNow(Error("Expected a Drivefs URL"));
  }

  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
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
          base::BindOnce(
              &ImageLoaderPrivateGetDriveThumbnailFunction::GotThumbnail, this),
          std::nullopt));
  return RespondLater();
}

void ImageLoaderPrivateGetDriveThumbnailFunction::GotThumbnail(
    const std::optional<std::vector<uint8_t>>& data) {
  if (!data) {
    Respond(WithArguments(""));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MakeThumbnailDataUrlOnThreadPool, kMimeTypeImagePng,
                     *data),
      base::BindOnce(
          &ImageLoaderPrivateGetDriveThumbnailFunction::SendEncodedThumbnail,
          this));
}

ImageLoaderPrivateGetPdfThumbnailFunction::
    ImageLoaderPrivateGetPdfThumbnailFunction() = default;

ImageLoaderPrivateGetPdfThumbnailFunction::
    ~ImageLoaderPrivateGetPdfThumbnailFunction() = default;

ExtensionFunction::ResponseAction
ImageLoaderPrivateGetPdfThumbnailFunction::Run() {
  using extensions::api::image_loader_private::GetPdfThumbnail::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(
          Profile::FromBrowserContext(browser_context()));
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURLInFirstPartyContext(url);

  if (file_system_url.type() != storage::kFileSystemTypeLocal) {
    return RespondNow(Error("Expected a native local URL"));
  }

  base::FilePath path =
      file_manager::util::GetLocalPathFromURL(file_system_context, url);
  if (path.empty() ||
      base::FilePath::CompareIgnoreCase(path.Extension(), ".pdf") != 0) {
    return RespondNow(Error("Can only handle PDF files"));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadLocalPdf, std::move(path)),
      base::BindOnce(&ImageLoaderPrivateGetPdfThumbnailFunction::FetchThumbnail,
                     this, gfx::Size(params->width, params->height)));
  return RespondLater();
}

void ImageLoaderPrivateGetPdfThumbnailFunction::FetchThumbnail(
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
  base::as_writable_chars(base::span(pdf_region.mapping)).copy_from(content);
  DCHECK(!pdf_thumbnailer_.is_bound());
  GetPdfService()->BindPdfThumbnailer(
      pdf_thumbnailer_.BindNewPipeAndPassReceiver());
  pdf_thumbnailer_.set_disconnect_handler(base::BindOnce(
      &ImageLoaderPrivateGetPdfThumbnailFunction::ThumbnailDisconnected,
      base::Unretained(this)));
  const PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  if (prefs && prefs->IsManagedPreference(prefs::kPdfUseSkiaRendererEnabled)) {
    pdf_thumbnailer_->SetUseSkiaRendererPolicy(
        prefs->GetBoolean(prefs::kPdfUseSkiaRendererEnabled));
  }
  auto params = pdf::mojom::ThumbParams::New(
      /*size_px=*/size, /*dpi=*/gfx::Size(kDpi, kDpi),
      /*stretch_to_bounds=*/false, /*keep_aspect_ratio=*/true);
  pdf_thumbnailer_->GetThumbnail(
      std::move(params), std::move(pdf_region.region),
      base::BindOnce(&ImageLoaderPrivateGetPdfThumbnailFunction::GotThumbnail,
                     this));
}

void ImageLoaderPrivateGetPdfThumbnailFunction::ThumbnailDisconnected() {
  DLOG(WARNING) << "PDF thumbnail disconnected";
  Respond(Error("PDF service disconnected"));
}

void ImageLoaderPrivateGetPdfThumbnailFunction::GotThumbnail(
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pdf_thumbnailer_.reset();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ConvertAndEncode, bitmap),
      base::BindOnce(
          &ImageLoaderPrivateGetPdfThumbnailFunction::SendEncodedThumbnail,
          this));
}

ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
    ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction() = default;

ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
    ~ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction() = default;

ExtensionFunction::ResponseAction
ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::Run() {
  using extensions::api::image_loader_private::
      GetArcDocumentsProviderThumbnail::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(
          Profile::FromBrowserContext(browser_context()));
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURLInFirstPartyContext(url);

  auto* root_map =
      arc::ArcDocumentsProviderRootMap::GetForBrowserContext(browser_context());
  if (!root_map) {
    return RespondNow(Error("File not found"));
  }
  base::FilePath path;
  auto* root = root_map->ParseAndLookup(file_system_url, &path);
  if (!root) {
    return RespondNow(Error("File not found"));
  }

  const gfx::Size size_hint(params->width_hint, params->height_hint);
  root->GetExtraFileMetadata(
      path, base::BindOnce(
                &ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
                    OnGetExtraFileMetadata,
                this, size_hint, file_system_url));
  return RespondLater();
}

void ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
    OnGetExtraFileMetadata(
        const gfx::Size& size_hint,
        const storage::FileSystemURL& file_system_url,
        base::File::Error result,
        const arc::ArcDocumentsProviderRoot::ExtraFileMetadata& metadata) {
  if (result != base::File::FILE_OK) {
    Respond(Error(base::File::ErrorToString(result)));
    return;
  }

  if (!metadata.supports_thumbnail) {
    Respond(WithArguments(""));
    return;
  }

  file_manager::util::ConvertToContentUrls(
      Profile::FromBrowserContext(browser_context()),
      std::vector<storage::FileSystemURL>{file_system_url},
      base::BindOnce(
          &ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
              GotContentUrls,
          this, size_hint));
}

void ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::GotContentUrls(
    const gfx::Size& size_hint,
    const std::vector<GURL>& urls,
    const std::vector<base::FilePath>& paths_to_share) {
  if (urls.size() != 1 || urls[0] == GURL()) {
    Respond(Error("Failed to resolve to countent URL"));
    return;
  }
  if (!paths_to_share.empty()) {
    Respond(
        Error("paths_to_share should be empty when getting "
              "ArcDocumentsProviderThumbnail URL"));
    return;
  }

  auto* runner = arc::ArcFileSystemOperationRunner::GetForBrowserContext(
      browser_context());
  if (!runner) {
    Respond(Error("ArcFileSystemOperationRunner is unavailable"));
    return;
  }
  const auto& url = urls[0];
  runner->OpenThumbnail(
      url, size_hint,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
                  GotArcThumbnailFileHandle,
              this),
          mojo::ScopedHandle()));
}

void ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
    GotArcThumbnailFileHandle(mojo::ScopedHandle handle) {
  mojo::PlatformHandle platform_handle =
      mojo::UnwrapPlatformHandle(std::move(handle));

  if (!platform_handle.is_valid()) {
    Respond(Error("File not found"));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadMojoHandleToDataUrl, std::move(platform_handle)),
      base::BindOnce(
          &ImageLoaderPrivateGetArcDocumentsProviderThumbnailFunction::
              SendEncodedThumbnail,
          this));
}

}  // namespace extensions

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print_spooler/print_session_impl.h"

#include <limits>
#include <utility>

#include "ash/public/cpp/arc_custom_tab.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/print_spooler/arc_print_spooler_util.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printing_service.h"
#include "components/arc/mojom/print_common.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/filename_util.h"
#include "printing/page_range.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "printing/units.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/size.h"

namespace arc {

namespace {

constexpr int kMinimumPdfSize = 50;

// Converts a color mode to its Mojo type.
mojom::PrintColorMode ToArcColorMode(int color_mode) {
  return printing::IsColorModelSelected(color_mode)
             ? mojom::PrintColorMode::COLOR
             : mojom::PrintColorMode::MONOCHROME;
}

// Converts a duplex mode to its Mojo type.
mojom::PrintDuplexMode ToArcDuplexMode(int duplex_mode) {
  switch (duplex_mode) {
    case printing::LONG_EDGE:
      return mojom::PrintDuplexMode::LONG_EDGE;
    case printing::SHORT_EDGE:
      return mojom::PrintDuplexMode::SHORT_EDGE;
    default:
      return mojom::PrintDuplexMode::NONE;
  }
}

// Gets and builds the print attributes from the job settings.
mojom::PrintAttributesPtr GetPrintAttributes(const base::Value& job_settings) {
  // PrintMediaSize:
  const base::Value* media_size_value =
      job_settings.FindDictKey(printing::kSettingMediaSize);
  if (!media_size_value)
    return nullptr;
  // Vendor ID will be empty when Destination is Save as PDF.
  const std::string* vendor_id =
      media_size_value->FindStringKey(printing::kSettingMediaSizeVendorId);
  std::string id = "PDF";
  if (vendor_id && !vendor_id->empty()) {
    id = *vendor_id;
  }
  base::Optional<int> width_microns =
      media_size_value->FindIntKey(printing::kSettingMediaSizeWidthMicrons);
  base::Optional<int> height_microns =
      media_size_value->FindIntKey(printing::kSettingMediaSizeHeightMicrons);
  if (!width_microns.has_value() || !height_microns.has_value())
    return nullptr;
  // Swap the width and height if layout is landscape.
  base::Optional<bool> landscape =
      job_settings.FindBoolKey(printing::kSettingLandscape);
  if (!landscape.has_value())
    return nullptr;
  gfx::Size size_micron;
  if (landscape.value()) {
    size_micron = gfx::Size(height_microns.value(), width_microns.value());
  } else {
    size_micron = gfx::Size(width_microns.value(), height_microns.value());
  }
  gfx::Size size_mil =
      gfx::ScaleToRoundedSize(size_micron, 1.0f / printing::kMicronsPerMil);
  mojom::PrintMediaSizePtr media_size = mojom::PrintMediaSize::New(
      id, "ARC", size_mil.width(), size_mil.height());

  // PrintResolution:
  int horizontal_dpi = job_settings.FindIntKey(printing::kSettingDpiHorizontal)
                           .value_or(printing::kDefaultPdfDpi);
  int vertical_dpi = job_settings.FindIntKey(printing::kSettingDpiVertical)
                         .value_or(printing::kDefaultPdfDpi);

  // PrintMargins:
  // Chrome uses margins to fit content to the printable area. Android uses
  // margins to crop content to the printable area. Set margins to 0 to prevent
  // cropping.
  mojom::PrintMarginsPtr margins = mojom::PrintMargins::New(0, 0, 0, 0);

  // PrintColorMode:
  base::Optional<int> color = job_settings.FindIntKey(printing::kSettingColor);
  if (!color.has_value())
    return nullptr;
  mojom::PrintColorMode color_mode = ToArcColorMode(color.value());

  // PrintDuplexMode:
  base::Optional<int> duplex =
      job_settings.FindIntKey(printing::kSettingDuplexMode);
  if (!duplex.has_value())
    return nullptr;
  mojom::PrintDuplexMode duplex_mode = ToArcDuplexMode(duplex.value());

  return mojom::PrintAttributes::New(
      std::move(media_size), gfx::Size(horizontal_dpi, vertical_dpi),
      std::move(margins), color_mode, duplex_mode);
}

// Creates a PrintDocumentRequest from the provided |job_settings|. Uses helper
// functions to parse |job_settings|.
mojom::PrintDocumentRequestPtr PrintDocumentRequestFromJobSettings(
    const base::Value& job_settings) {
  return mojom::PrintDocumentRequest::New(
      printing::GetPageRangesFromJobSettings(job_settings),
      GetPrintAttributes(job_settings));
}

// Uses the provided ScopedHandle to read a preview document from ARC into
// read-only shared memory.
base::ReadOnlySharedMemoryRegion ReadPreviewDocument(
    mojo::ScopedHandle preview_document,
    size_t data_size) {
  base::PlatformFile platform_file;
  if (mojo::UnwrapPlatformFile(std::move(preview_document), &platform_file) !=
      MOJO_RESULT_OK) {
    return base::ReadOnlySharedMemoryRegion();
  }

  base::File src_file(platform_file);
  if (!src_file.IsValid()) {
    DPLOG(ERROR) << "Source file is invalid.";
    return base::ReadOnlySharedMemoryRegion();
  }

  base::MappedReadOnlyRegion region_mapping =
      mojo::CreateReadOnlySharedMemoryRegion(data_size);
  if (!region_mapping.IsValid())
    return std::move(region_mapping.region);

  bool success = src_file.ReadAndCheck(
      0, region_mapping.mapping.GetMemoryAsSpan<uint8_t>());
  if (!success) {
    DPLOG(ERROR) << "Error reading PDF.";
    return base::ReadOnlySharedMemoryRegion();
  }

  return std::move(region_mapping.region);
}

}  // namespace

// static
mojom::PrintSessionHostPtr PrintSessionImpl::Create(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab,
    mojom::PrintSessionInstancePtr instance) {
  if (!custom_tab || !instance)
    return nullptr;

  // This object will be deleted when the mojo connection is closed.
  mojom::PrintSessionHostPtr ptr;
  new PrintSessionImpl(std::move(web_contents), std::move(custom_tab),
                       std::move(instance), mojo::MakeRequest(&ptr));
  return ptr;
}

PrintSessionImpl::PrintSessionImpl(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab,
    mojom::PrintSessionInstancePtr instance,
    mojom::PrintSessionHostRequest request)
    : ArcCustomTabModalDialogHost(std::move(custom_tab),
                                  std::move(web_contents)),
      instance_(std::move(instance)),
      print_renderer_binding_(this),
      session_binding_(this, std::move(request)) {
  session_binding_.set_connection_error_handler(
      base::BindOnce(&PrintSessionImpl::Close, weak_ptr_factory_.GetWeakPtr()));
  web_contents_->SetUserData(UserDataKey(), base::WrapUnique(this));

  aura::Window* window = web_contents_->GetNativeView();
  custom_tab_->Attach(window);
  window->Show();

  // TODO(jschettler): Handle this correctly once crbug.com/636642 is
  // resolved. Until then, give the PDF plugin time to load.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrintSessionImpl::StartPrintAfterDelay,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

PrintSessionImpl::~PrintSessionImpl() {
  // Delete the saved print document now that it's no longer needed.
  base::FilePath file_path;
  if (!net::FileURLToFilePath(web_contents_->GetVisibleURL(), &file_path)) {
    LOG(ERROR) << "Failed to obtain file path from URL.";
    return;
  }

  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&DeletePrintDocument, file_path));
}

void PrintSessionImpl::CreatePreviewDocument(
    base::Value job_settings,
    CreatePreviewDocumentCallback callback) {
  mojom::PrintDocumentRequestPtr request =
      PrintDocumentRequestFromJobSettings(job_settings);
  if (!request || !request->attributes) {
    std::move(callback).Run(base::ReadOnlySharedMemoryRegion());
    return;
  }

  instance_->CreatePreviewDocument(
      std::move(request),
      base::BindOnce(&PrintSessionImpl::OnPreviewDocumentCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintSessionImpl::OnPreviewDocumentCreated(
    CreatePreviewDocumentCallback callback,
    mojo::ScopedHandle preview_document,
    int64_t data_size) {
  if (data_size < kMinimumPdfSize ||
      !base::IsValueInRangeForNumericType<size_t>(data_size)) {
    std::move(callback).Run(base::ReadOnlySharedMemoryRegion());
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&ReadPreviewDocument, std::move(preview_document),
                     static_cast<size_t>(data_size)),
      base::BindOnce(&PrintSessionImpl::OnPreviewDocumentRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintSessionImpl::OnPreviewDocumentRead(
    CreatePreviewDocumentCallback callback,
    base::ReadOnlySharedMemoryRegion preview_document_region) {
  if (!preview_document_region.IsValid()) {
    std::move(callback).Run(std::move(preview_document_region));
    return;
  }

  if (!pdf_flattener_.is_bound()) {
    GetPrintingService()->BindPdfFlattener(
        pdf_flattener_.BindNewPipeAndPassReceiver());
  }
  pdf_flattener_->FlattenPdf(
      std::move(preview_document_region),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), base::ReadOnlySharedMemoryRegion()));
}

void PrintSessionImpl::Close() {
  web_contents_->RemoveUserData(UserDataKey());
}

void PrintSessionImpl::OnPrintPreviewClosed() {
  instance_->OnPrintPreviewClosed();
}

void PrintSessionImpl::StartPrintAfterDelay() {
  printing::mojom::PrintRendererAssociatedPtrInfo print_renderer_ptr_info;
  print_renderer_binding_.Bind(mojo::MakeRequest(&print_renderer_ptr_info));
  printing::StartPrint(web_contents_.get(), std::move(print_renderer_ptr_info),
                       false, false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintSessionImpl)

}  // namespace arc

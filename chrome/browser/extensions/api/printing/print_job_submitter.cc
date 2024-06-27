// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/print_job_submitter.h"

#include <cstring>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/printing/printing_api_utils.h"
#include "chrome/browser/printing/pdf_blob_data_flattener.h"
#include "chrome/browser/printing/print_job_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blob_reader.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printing_utils.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/native_window_tracker.h"

namespace extensions {

namespace {

constexpr char kPdfMimeType[] = "application/pdf";
constexpr char kPngMimeType[] = "image/png";

constexpr char kUnsupportedContentType[] = "Unsupported content type";
constexpr char kInvalidTicket[] = "Invalid ticket";
constexpr char kInvalidPrinterId[] = "Invalid printer ID";
constexpr char kPrinterUnavailable[] = "Printer is unavailable at the moment";
constexpr char kUnsupportedTicket[] =
    "Ticket is unsupported on the given printer";
constexpr char kInvalidData[] = "Invalid document";
constexpr char kPrintingFailed[] = "Printing failed";

constexpr int kIconSize = 64;

// There is no easy way to interact with UI dialogs, so we want to have an
// ability to skip this stage for browser tests.
bool g_skip_confirmation_dialog_for_testing = false;

// Returns true if print job request dialog should be shown.
bool IsUserConfirmationRequired(content::BrowserContext* browser_context,
                                const std::string& extension_id) {
  if (g_skip_confirmation_dialog_for_testing)
    return false;
  const base::Value::List& list =
      Profile::FromBrowserContext(browser_context)
          ->GetPrefs()
          ->GetList(prefs::kPrintingAPIExtensionsAllowlist);
  return !base::Contains(list, base::Value(extension_id));
}

}  // namespace

PrintJobSubmitter::PrintJobSubmitter(
    gfx::NativeWindow native_window,
    content::BrowserContext* browser_context,
    printing::PrintJobController* print_job_controller,
    printing::PdfBlobDataFlattener* pdf_blob_data_flattener,
    scoped_refptr<const extensions::Extension> extension,
    api::printing::SubmitJobRequest request,
    crosapi::mojom::LocalPrinter* local_printer,
    SubmitJobCallback callback)
    : native_window_(native_window),
      browser_context_(browser_context),
      print_job_controller_(print_job_controller),
      pdf_blob_data_flattener_(*pdf_blob_data_flattener),
      extension_(extension),
      request_(std::move(request)),
      local_printer_(local_printer),
      callback_(std::move(callback)) {
  DCHECK(extension);
  if (native_window)
    native_window_tracker_ = views::NativeWindowTracker::Create(native_window);
}

PrintJobSubmitter::~PrintJobSubmitter() = default;

// static
void PrintJobSubmitter::Run(std::unique_ptr<PrintJobSubmitter> submitter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(submitter->callback_);
  PrintJobSubmitter* ptr = submitter.get();
  ptr->callback_ = std::move(ptr->callback_)
                       .Then(base::OnceClosure(
                           base::DoNothingWithBoundArgs(std::move(submitter))));
  ptr->Start();
}

void PrintJobSubmitter::Start() {
  if (!CheckContentType()) {
    FireErrorCallback(kUnsupportedContentType);
    return;
  }
  if (!CheckPrintTicket()) {
    FireErrorCallback(kInvalidTicket);
    return;
  }
  CheckPrinter();
}

bool PrintJobSubmitter::CheckContentType() const {
  return request_.job.content_type == kPdfMimeType ||
         request_.job.content_type == kPngMimeType;
}

bool PrintJobSubmitter::CheckPrintTicket() {
  settings_ = ParsePrintTicket(request_.job.ticket.ToValue());
  if (!settings_)
    return false;
  settings_->set_title(base::UTF8ToUTF16(request_.job.title));
  settings_->set_device_name(base::UTF8ToUTF16(request_.job.printer_id));
  return true;
}

void PrintJobSubmitter::CheckPrinter() {
  CHECK(local_printer_);
  local_printer_->GetCapability(
      request_.job.printer_id,
      base::BindOnce(&PrintJobSubmitter::CheckCapabilitiesCompatibility,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::CheckCapabilitiesCompatibility(
    crosapi::mojom::CapabilitiesResponsePtr caps) {
  if (!caps) {
    FireErrorCallback(kInvalidPrinterId);
    return;
  }
  printer_name_ = base::UTF8ToUTF16(caps->basic_info->name);
  if (!caps->capabilities) {
    FireErrorCallback(kPrinterUnavailable);
    return;
  }
  if (!CheckSettingsAndCapabilitiesCompatibility(*settings_,
                                                 *caps->capabilities)) {
    FireErrorCallback(kUnsupportedTicket);
    return;
  }
  ReadDocumentData();
}

void PrintJobSubmitter::ReadDocumentData() {
  CHECK(request_.document_blob_uuid);
  if (request_.job.content_type == kPdfMimeType) {
    pdf_blob_data_flattener_->ReadAndFlattenPdf(
        browser_context_->GetBlobRemote(*request_.document_blob_uuid),
        base::BindOnce(&PrintJobSubmitter::OnPdfReadAndFlattened,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    BlobReader::Read(
        browser_context_->GetBlobRemote(*request_.document_blob_uuid),
        base::BindOnce(&PrintJobSubmitter::OnImageDataRead,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PrintJobSubmitter::OnPdfReadAndFlattened(
    std::unique_ptr<printing::FlattenPdfResult> result) {
  if (!result) {
    FireErrorCallback(kInvalidData);
    return;
  }

  flatten_pdf_result_ = std::move(result);

  // Directly submit the job if the extension is allowed.
  if (!IsUserConfirmationRequired(browser_context_, extension_->id())) {
    StartPrintJob();
    return;
  }
  extensions::ImageLoader::Get(browser_context_)
      ->LoadImageAtEveryScaleFactorAsync(
          extension_.get(), gfx::Size(kIconSize, kIconSize),
          base::BindOnce(&PrintJobSubmitter::ShowPrintJobConfirmationDialog,
                         weak_ptr_factory_.GetWeakPtr()));
}

// Handle PNG input by converting it to PDF, and then sending the resulting
// PDF to the printer like we would if it had been submitted directly.
void PrintJobSubmitter::OnImageDataRead(std::string data,
                                        int64_t /*blob_total_size*/) {
  sk_sp<SkData> image_data = SkData::MakeWithCopy(data.data(), data.size());
  std::unique_ptr<SkCodec> codec = SkPngDecoder::Decode(image_data, nullptr);
  if (!codec) {
    LOG(WARNING) << "Failed to decode PNG";
    FireErrorCallback(kInvalidData);
    return;
  }

  auto img_tuple = codec->getImage();
  CHECK(std::get<1>(img_tuple) == SkCodec::Result::kSuccess);
  sk_sp<SkImage> image = std::get<0>(img_tuple);

  SkDynamicMemoryWStream buffer;
  SkPDF::Metadata metadata;
  auto pdf_document = SkPDF::MakeDocument(&buffer, metadata);
  CHECK(pdf_document);
  SkCanvas* canvas = pdf_document->beginPage(image->width(), image->height());
  canvas->drawImage(image, 0, 0);
  pdf_document->endPage();
  pdf_document->close();

  // The generated PDF consists of a single image and does not contain forms,
  // JavaScript, etc. So it is already flattened, and can be treated as such.
  sk_sp<SkData> pdf_data = buffer.detachAsData();
  auto metafile = std::make_unique<printing::MetafileSkia>();
  CHECK(metafile->InitFromData({pdf_data->bytes(), pdf_data->size()}));
  OnPdfReadAndFlattened(
      std::make_unique<printing::FlattenPdfResult>(std::move(metafile), 1));
}

void PrintJobSubmitter::ShowPrintJobConfirmationDialog(
    const gfx::Image& extension_icon) {
  // If the browser window was closed during API request handling, change
  // |native_window_| appropriately.
  if (native_window_tracker_ &&
      native_window_tracker_->WasNativeWindowDestroyed())
    native_window_ = gfx::NativeWindow();

  extensions::ShowPrintJobConfirmationDialog(
      native_window_, extension_->id(), base::UTF8ToUTF16(extension_->name()),
      extension_icon.AsImageSkia(), settings_->title(), printer_name_,
      base::BindOnce(&PrintJobSubmitter::OnPrintJobConfirmationDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnPrintJobConfirmationDialogClosed(bool accepted) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  // If the user hasn't accepted a print job or the extension is
  // unloaded/disabled by the time the dialog is closed, reject the request.
  if (!accepted || !ExtensionRegistry::Get(browser_context_)
                        ->enabled_extensions()
                        .Contains(extension_->id())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), base::unexpected(std::nullopt)));
    return;
  }
  StartPrintJob();
}

void PrintJobSubmitter::StartPrintJob() {
  CHECK(extension_);
  CHECK(settings_);
  CHECK(flatten_pdf_result_);

  auto flatten_pdf_result = std::move(flatten_pdf_result_);
  uint32_t page_count = flatten_pdf_result->page_count;
  print_job_controller_->CreatePrintJob(
      std::move(flatten_pdf_result->flattened_pdf), std::move(settings_),
      page_count, crosapi::mojom::PrintJob::Source::kExtension,
      extension_->id(),
      base::BindOnce(&PrintJobSubmitter::OnPrintJobCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnPrintJobCreated(
    std::optional<printing::PrintJobCreatedInfo> info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!info) {
    FireErrorCallback(kPrintingFailed);
    return;
  }
  DCHECK(callback_);
  std::move(callback_).Run(std::move(*info));
}

void PrintJobSubmitter::FireErrorCallback(const std::string& error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), base::unexpected(error)));
}

// static
base::AutoReset<bool> PrintJobSubmitter::DisablePdfFlatteningForTesting() {
  return printing::PdfBlobDataFlattener::DisablePdfFlatteningForTesting();
}

// static
void PrintJobSubmitter::SkipConfirmationDialogForTesting() {
  g_skip_confirmation_dialog_for_testing = true;
}

}  // namespace extensions

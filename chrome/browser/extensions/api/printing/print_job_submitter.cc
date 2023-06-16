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
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/printing/print_job_controller.h"
#include "chrome/browser/extensions/api/printing/printing_api_utils.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blob_reader.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printing_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/native_window_tracker.h"

namespace extensions {

namespace {

constexpr char kPdfMimeType[] = "application/pdf";

constexpr char kUnsupportedContentType[] = "Unsupported content type";
constexpr char kInvalidTicket[] = "Invalid ticket";
constexpr char kInvalidPrinterId[] = "Invalid printer ID";
constexpr char kPrinterUnavailable[] = "Printer is unavailable at the moment";
constexpr char kUnsupportedTicket[] =
    "Ticket is unsupported on the given printer";
constexpr char kInvalidData[] = "Invalid document";
constexpr char kPrintingFailed[] = "Printing failed";

constexpr int kIconSize = 64;

// We want to have an ability to disable PDF flattening for unit tests as
// printing::mojom::PdfFlattener requires real browser instance to be able to
// handle requests.
bool g_disable_pdf_flattening_for_testing = false;

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
    PrintJobController* print_job_controller,
    mojo::Remote<printing::mojom::PdfFlattener>* pdf_flattener,
    scoped_refptr<const extensions::Extension> extension,
    api::printing::SubmitJobRequest request,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    int local_printer_version,
#endif
    crosapi::mojom::LocalPrinter* local_printer,
    SubmitJobCallback callback)
    : native_window_(native_window),
      browser_context_(browser_context),
      print_job_controller_(print_job_controller),
      pdf_flattener_(pdf_flattener),
      extension_(extension),
      request_(std::move(request)),
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      local_printer_version_(local_printer_version),
#endif
      local_printer_(local_printer),
      callback_(std::move(callback)) {
  DCHECK(extension);
  if (native_window)
    native_window_tracker_ = views::NativeWindowTracker::Create(native_window);
}

PrintJobSubmitter::~PrintJobSubmitter() {
  DCHECK(!callback_);
  if (print_job_)
    print_job_->RemoveObserver(*this);
}

// static
void PrintJobSubmitter::Run(std::unique_ptr<PrintJobSubmitter> submitter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(submitter->callback_);
  PrintJobSubmitter* ptr = submitter.get();
  ptr->callback_ =
      std::move(ptr->callback_)
          .Then(base::BindOnce([](std::unique_ptr<PrintJobSubmitter>) {},
                               std::move(submitter)));
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
  return request_.job.content_type == kPdfMimeType;
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
  if (!local_printer_) {
    LOG(ERROR)
        << "Local printer not available (PrintJobSubmitter::CheckPrinter()";
    CheckCapabilitiesCompatibility(nullptr);
    return;
  }
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
  DCHECK(request_.document_blob_uuid);
  BlobReader::Read(browser_context_, *request_.document_blob_uuid,
                   base::BindOnce(&PrintJobSubmitter::OnDocumentDataRead,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnDocumentDataRead(std::unique_ptr<std::string> data,
                                           int64_t total_blob_length) {
  if (!data || !printing::LooksLikePdf(*data)) {
    FireErrorCallback(kInvalidData);
    return;
  }

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(data->length());
  if (!memory.IsValid()) {
    FireErrorCallback(kInvalidData);
    return;
  }
  memcpy(memory.mapping.memory(), data->data(), data->length());

  if (g_disable_pdf_flattening_for_testing) {
    OnPdfFlattened(std::move(memory.region));
    return;
  }

  if (!pdf_flattener_->is_bound()) {
    GetPrintingService()->BindPdfFlattener(
        pdf_flattener_->BindNewPipeAndPassReceiver());
    pdf_flattener_->set_disconnect_handler(
        base::BindOnce(&PrintJobSubmitter::OnPdfFlattenerDisconnected,
                       weak_ptr_factory_.GetWeakPtr()));
    const PrefService* prefs =
        Profile::FromBrowserContext(browser_context_)->GetPrefs();
    if (prefs &&
        prefs->IsManagedPreference(prefs::kPdfUseSkiaRendererEnabled)) {
      (*pdf_flattener_)
          ->SetUseSkiaRendererPolicy(
              prefs->GetBoolean(prefs::kPdfUseSkiaRendererEnabled));
    }
  }
  (*pdf_flattener_)
      ->FlattenPdf(std::move(memory.region),
                   base::BindOnce(&PrintJobSubmitter::OnPdfFlattened,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnPdfFlattenerDisconnected() {
  FireErrorCallback(kInvalidData);
}

void PrintJobSubmitter::OnPdfFlattened(
    base::ReadOnlySharedMemoryRegion flattened_pdf) {
  auto mapping = flattened_pdf.Map();
  if (!mapping.IsValid()) {
    FireErrorCallback(kInvalidData);
    return;
  }

  flattened_pdf_mapping_ = std::move(mapping);

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
        FROM_HERE, base::BindOnce(std::move(callback_), absl::nullopt, nullptr,
                                  nullptr, absl::nullopt));
    return;
  }
  StartPrintJob();
}

void PrintJobSubmitter::StartPrintJob() {
  DCHECK(extension_);
  DCHECK(settings_);
  auto metafile = std::make_unique<printing::MetafileSkia>();
  CHECK(metafile->InitFromData(
      flattened_pdf_mapping_.GetMemoryAsSpan<const uint8_t>()));
  CHECK(!print_job_);
  print_job_ = print_job_controller_->StartPrintJob(
      extension_->id(), std::move(metafile), std::move(settings_));
  print_job_->AddObserver(*this);
}

void PrintJobSubmitter::OnDocDone(int job_id,
                                  printing::PrintedDocument* document) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  std::move(callback_).Run(job_id, print_job_.get(), document, absl::nullopt);
}

void PrintJobSubmitter::OnFailed() {
  FireErrorCallback(kPrintingFailed);
}

void PrintJobSubmitter::FireErrorCallback(const std::string& error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), absl::nullopt, nullptr,
                                nullptr, error));
}

// static
base::AutoReset<bool> PrintJobSubmitter::DisablePdfFlatteningForTesting() {
  return base::AutoReset<bool>(&g_disable_pdf_flattening_for_testing, true);
}

// static
base::AutoReset<bool> PrintJobSubmitter::SkipConfirmationDialogForTesting() {
  return base::AutoReset<bool>(&g_skip_confirmation_dialog_for_testing, true);
}

}  // namespace extensions

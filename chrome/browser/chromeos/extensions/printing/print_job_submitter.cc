// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/print_job_submitter.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/printing/print_job_controller.h"
#include "chrome/browser/chromeos/extensions/printing/printer_capabilities_provider.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_utils.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/blob_reader.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

constexpr char kPdfMimeType[] = "application/pdf";

// PDF document format identifier.
constexpr char kPdfMagicBytes[] = "%PDF";

constexpr char kUnsupportedContentType[] = "Unsupported content type";
constexpr char kInvalidTicket[] = "Invalid ticket";
constexpr char kInvalidPrinterId[] = "Invalid printer ID";
constexpr char kPrinterUnavailable[] = "Printer is unavailable at the moment";
constexpr char kUnsupportedTicket[] =
    "Ticket is unsupported on the given printer";
constexpr char kInvalidData[] = "Invalid document";

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
  const base::ListValue* list =
      Profile::FromBrowserContext(browser_context)
          ->GetPrefs()
          ->GetList(prefs::kPrintingAPIExtensionsAllowlist);
  base::Value value(extension_id);
  return list->Find(value) == list->end();
}

}  // namespace

PrintJobSubmitter::PrintJobSubmitter(
    gfx::NativeWindow native_window,
    content::BrowserContext* browser_context,
    chromeos::CupsPrintersManager* printers_manager,
    PrinterCapabilitiesProvider* printer_capabilities_provider,
    PrintJobController* print_job_controller,
    mojo::Remote<printing::mojom::PdfFlattener>* pdf_flattener,
    scoped_refptr<const extensions::Extension> extension,
    api::printing::SubmitJobRequest request)
    : native_window_(native_window),
      browser_context_(browser_context),
      printers_manager_(printers_manager),
      printer_capabilities_provider_(printer_capabilities_provider),
      print_job_controller_(print_job_controller),
      pdf_flattener_(pdf_flattener),
      extension_(extension),
      request_(std::move(request)) {
  DCHECK(extension);
  if (native_window)
    native_window_tracker_ = NativeWindowTracker::Create(native_window);
}

PrintJobSubmitter::~PrintJobSubmitter() = default;

void PrintJobSubmitter::Start(SubmitJobCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
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
  settings_ = ParsePrintTicket(
      base::Value::FromUniquePtrValue(request_.job.ticket.ToValue()));
  if (!settings_)
    return false;
  settings_->set_title(base::UTF8ToUTF16(request_.job.title));
  settings_->set_device_name(base::UTF8ToUTF16(request_.job.printer_id));
  return true;
}

void PrintJobSubmitter::CheckPrinter() {
  base::Optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(request_.job.printer_id);
  if (!printer) {
    FireErrorCallback(kInvalidPrinterId);
    return;
  }
  printer_name_ = base::UTF8ToUTF16(printer->display_name());
  printer_capabilities_provider_->GetPrinterCapabilities(
      request_.job.printer_id,
      base::BindOnce(&PrintJobSubmitter::CheckCapabilitiesCompatibility,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::CheckCapabilitiesCompatibility(
    base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities) {
  if (!capabilities) {
    FireErrorCallback(kPrinterUnavailable);
    return;
  }
  if (!CheckSettingsAndCapabilitiesCompatibility(*settings_, *capabilities)) {
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
  if (!data ||
      !base::StartsWith(*data, kPdfMagicBytes, base::CompareCase::SENSITIVE)) {
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
  if (native_window_tracker_ && native_window_tracker_->WasNativeWindowClosed())
    native_window_ = gfx::kNullNativeWindow;

  chrome::ShowPrintJobConfirmationDialog(
      native_window_, extension_->id(), base::UTF8ToUTF16(extension_->name()),
      extension_icon.AsImageSkia(), settings_->title(), printer_name_,
      base::BindOnce(&PrintJobSubmitter::OnPrintJobConfirmationDialogClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnPrintJobConfirmationDialogClosed(bool accepted) {
  // If the user hasn't accepted a print job or the extension is
  // unloaded/disabled by the time the dialog is closed, reject the request.
  if (!accepted || !ExtensionRegistry::Get(browser_context_)
                        ->enabled_extensions()
                        .Contains(extension_->id())) {
    OnPrintJobRejected();
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
  print_job_controller_->StartPrintJob(
      extension_->id(), std::move(metafile), std::move(settings_),
      base::BindOnce(&PrintJobSubmitter::OnPrintJobSubmitted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnPrintJobRejected() {
  DCHECK(callback_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_),
                                api::printing::SUBMIT_JOB_STATUS_USER_REJECTED,
                                nullptr, base::nullopt));
}

void PrintJobSubmitter::OnPrintJobSubmitted(
    std::unique_ptr<std::string> job_id) {
  DCHECK(job_id);
  DCHECK(callback_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), api::printing::SUBMIT_JOB_STATUS_OK,
                     std::move(job_id), base::nullopt));
}

void PrintJobSubmitter::FireErrorCallback(const std::string& error) {
  DCHECK(callback_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), base::nullopt, nullptr, error));
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

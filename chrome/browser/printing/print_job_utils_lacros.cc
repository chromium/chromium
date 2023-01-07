// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_utils_lacros.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace printing {

namespace {

crosapi::mojom::PrintJobPtr PrintJobToMojom(int job_id,
                                            const PrintedDocument& document,
                                            PrintJob::Source source,
                                            const std::string& source_id) {
  std::u16string title = SimplifyDocumentTitle(document.name());
  if (title.empty()) {
    title = SimplifyDocumentTitle(
        l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE));
  }
  const PrintSettings& settings = document.settings();
  int duplex = static_cast<int>(settings.duplex_mode());
  DCHECK(duplex >= 0);
  DCHECK(duplex < 3);
  return crosapi::mojom::PrintJob::New(
      base::UTF16ToUTF8(settings.device_name()), base::UTF16ToUTF8(title),
      job_id, document.page_count(), source, source_id, settings.color(),
      static_cast<crosapi::mojom::PrintJob::DuplexMode>(duplex),
      settings.requested_media().size_microns,
      settings.requested_media().vendor_id, settings.copies());
}

}  // namespace

void NotifyAshJobCreated(const PrintJob& job,
                         int job_id,
                         const PrintedDocument& document,
                         crosapi::mojom::LocalPrinter* local_printer) {
  if (!local_printer) {
    LOG(ERROR) << "Could not report print job queued";
    return;
  }
  local_printer->CreatePrintJob(
      PrintJobToMojom(job_id, document, job.source(), job.source_id()),
      base::DoNothing());
}

void NotifyAshJobCreated(const PrintJob& job,
                         int job_id,
                         const PrintedDocument& document) {
  crosapi::mojom::LocalPrinter* local_printer = nullptr;
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<crosapi::mojom::LocalPrinter>())
    local_printer = service->GetRemote<crosapi::mojom::LocalPrinter>().get();
  NotifyAshJobCreated(job, job_id, document, local_printer);
}

}  // namespace printing

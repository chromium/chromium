// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/fake_print_job_controller.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#include "chrome/browser/printing/print_job.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

FakePrintJobController::FakePrintJobController() = default;

FakePrintJobController::~FakePrintJobController() = default;

void FakePrintJobController::CreatePrintJob(
    const std::string& printer_id,
    int job_id,
    const std::string& extension_id,
    crosapi::mojom::PrintJob::Source source) const {
  auto job = base::MakeRefCounted<printing::PrintJob>();
  job->SetSource(source, extension_id);
  auto settings = std::make_unique<printing::PrintSettings>();
  settings->set_device_name(base::UTF8ToUTF16(printer_id));
  auto document = base::MakeRefCounted<printing::PrintedDocument>(
      std::move(settings), std::u16string(), 0);
  auto details = base::MakeRefCounted<printing::JobEventDetails>(
      printing::JobEventDetails::DOC_DONE, job_id, document.get());
  // Normally, PrintingAPIHandler observes the DOC_DONE notification.
  handler_->Observe(chrome::NOTIFICATION_PRINT_JOB_EVENT,
                    content::Source<printing::PrintJob>(job.get()),
                    content::Details<printing::JobEventDetails>(details.get()));
}

void FakePrintJobController::StartPrintJob(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    std::unique_ptr<printing::PrintSettings> settings,
    StartPrintJobCallback callback) {
  std::string printer_id = base::UTF16ToUTF8(settings->device_name());
  CreatePrintJob(printer_id, ++job_id_, extension_id,
                 crosapi::mojom::PrintJob::Source::EXTENSION);
  std::string cups_id = PrintingAPIHandler::CreateUniqueId(printer_id, job_id_);
  EXPECT_TRUE(print_jobs_.contains(cups_id));
  std::move(callback).Run(std::make_unique<std::string>(std::move(cups_id)));
}

void FakePrintJobController::OnPrintJobCreated(const std::string& extension_id,
                                               const std::string& job_id) {
  EXPECT_FALSE(print_jobs_.contains(job_id));
  print_jobs_.insert(job_id);
}

void FakePrintJobController::OnPrintJobFinished(const std::string& job_id) {
  EXPECT_TRUE(print_jobs_.contains(job_id));
  print_jobs_.erase(job_id);
}

}  // namespace extensions

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/fake_print_job_controller.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/history/print_job_info_proto_conversions.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"

namespace extensions {

FakePrintJobController::FakePrintJobController(
    chromeos::TestCupsPrintJobManager* print_job_manager,
    chromeos::CupsPrintersManager* printers_manager)
    : print_job_manager_(print_job_manager),
      printers_manager_(printers_manager) {}

FakePrintJobController::~FakePrintJobController() = default;

void FakePrintJobController::StartPrintJob(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    std::unique_ptr<printing::PrintSettings> settings,
    StartPrintJobCallback callback) {
  absl::optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(base::UTF16ToUTF8(settings->device_name()));
  if (!printer) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Create and start new CupsPrintJob.
  job_id_++;
  auto print_job = std::make_unique<chromeos::CupsPrintJob>(
      *printer, job_id_, base::UTF16ToUTF8(settings->title()),
      /*total_page_number=*/1, printing::PrintJob::Source::EXTENSION,
      extension_id, chromeos::PrintSettingsToProto(*settings));
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->StartPrintJob(print_job.get());
  std::string print_job_unique_id = print_job->GetUniqueId();
  jobs_[print_job_unique_id] = std::move(print_job);

  std::move(callback).Run(std::make_unique<std::string>(print_job_unique_id));
}

bool FakePrintJobController::CancelPrintJob(const std::string& job_id) {
  auto it = jobs_.find(job_id);
  if (it == jobs_.end())
    return false;
  print_job_manager_->CancelPrintJob(it->second.get());
  return true;
}

void FakePrintJobController::OnPrintJobCreated(
    const std::string& extension_id,
    const std::string& job_id,
    base::WeakPtr<chromeos::CupsPrintJob> cups_job) {}

void FakePrintJobController::OnPrintJobFinished(const std::string& job_id) {
  jobs_.erase(job_id);
}

chromeos::CupsPrintJob* FakePrintJobController::GetCupsPrintJob(
    const std::string& job_id) {
  auto it = jobs_.find(job_id);
  if (it == jobs_.end())
    return nullptr;
  return it->second.get();
}

}  // namespace extensions

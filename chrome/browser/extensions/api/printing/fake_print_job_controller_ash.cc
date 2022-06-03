// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/fake_print_job_controller_ash.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/history/print_job_info_proto_conversions.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"

namespace extensions {

FakePrintJobControllerAsh::FakePrintJobControllerAsh(
    chromeos::TestCupsPrintJobManager* print_job_manager,
    chromeos::CupsPrintersManager* printers_manager)
    : print_job_manager_(print_job_manager),
      printers_manager_(printers_manager) {}

FakePrintJobControllerAsh::~FakePrintJobControllerAsh() = default;

void FakePrintJobControllerAsh::StartPrintJob(
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

  // Create a new CupsPrintJob.
  auto print_job = std::make_unique<chromeos::CupsPrintJob>(
      *printer, ++job_id_, base::UTF16ToUTF8(settings->title()),
      /*total_page_number=*/1, printing::PrintJob::Source::EXTENSION,
      extension_id, chromeos::PrintSettingsToProto(*settings));
  print_job_manager_->CreatePrintJob(print_job.get());
  std::move(callback).Run(
      std::make_unique<std::string>(print_job->GetUniqueId()));
  jobs_[print_job->GetUniqueId()] = std::move(print_job);

  // Notify DOC_DONE.
  auto job = base::MakeRefCounted<printing::PrintJob>();
  job->SetSource(crosapi::mojom::PrintJob::Source::EXTENSION, extension_id);
  auto document = base::MakeRefCounted<printing::PrintedDocument>(
      std::move(settings), std::u16string(), 0);
  auto details = base::MakeRefCounted<printing::JobEventDetails>(
      printing::JobEventDetails::DOC_DONE, job_id_, document.get());
  PrintingAPIHandler::Get(ProfileManager::GetPrimaryUserProfile())
      ->Observe(chrome::NOTIFICATION_PRINT_JOB_EVENT,
                content::Source<printing::PrintJob>(job.get()),
                content::Details<printing::JobEventDetails>(details.get()));
}

void FakePrintJobControllerAsh::OnPrintJobCreated(
    const std::string& extension_id,
    const std::string& job_id) {
  DCHECK(jobs_.contains(job_id));
  DCHECK(jobs_[job_id]);
  print_job_manager_->StartPrintJob(jobs_[job_id].get());
}

void FakePrintJobControllerAsh::OnPrintJobFinished(const std::string& job_id) {
  jobs_.erase(job_id);
}

chromeos::CupsPrintJob* FakePrintJobControllerAsh::GetCupsPrintJob(
    const std::string& job_id) {
  auto it = jobs_.find(job_id);
  if (it == jobs_.end())
    return nullptr;
  return it->second.get();
}

}  // namespace extensions

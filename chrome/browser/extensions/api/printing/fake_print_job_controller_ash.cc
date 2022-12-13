// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/printing/fake_print_job_controller_ash.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/history/print_job_info_proto_conversions.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Subclass PrintJob to allow construction without supplying a PrintJobManager
// instance.
class PrintJobForTesting : public printing::PrintJob {
 public:
  PrintJobForTesting() = default;

 private:
  ~PrintJobForTesting() override = default;
};

FakePrintJobControllerAsh::FakePrintJobControllerAsh(
    ash::TestCupsPrintJobManager* print_job_manager,
    ash::CupsPrintersManager* printers_manager)
    : print_job_manager_(print_job_manager),
      printers_manager_(printers_manager) {
  DCHECK(print_job_manager_);
  DCHECK(printers_manager_);
  print_job_manager_->AddObserver(this);
}

FakePrintJobControllerAsh::~FakePrintJobControllerAsh() {
  print_job_manager_->RemoveObserver(this);
}

void FakePrintJobControllerAsh::OnPrintJobDone(
    base::WeakPtr<ash::CupsPrintJob> job) {
  jobs_.erase(job->GetUniqueId());
}

void FakePrintJobControllerAsh::OnPrintJobError(
    base::WeakPtr<ash::CupsPrintJob> job) {
  jobs_.erase(job->GetUniqueId());
}

void FakePrintJobControllerAsh::OnPrintJobCancelled(
    base::WeakPtr<ash::CupsPrintJob> job) {
  jobs_.erase(job->GetUniqueId());
}

scoped_refptr<printing::PrintJob> FakePrintJobControllerAsh::StartPrintJob(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    std::unique_ptr<printing::PrintSettings> settings) {
  auto job = base::MakeRefCounted<PrintJobForTesting>();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FakePrintJobControllerAsh::StartPrinting,
                                weak_ptr_factory_.GetWeakPtr(), job,
                                extension_id, std::move(settings)));
  return job;
}

void FakePrintJobControllerAsh::StartPrinting(
    scoped_refptr<printing::PrintJob> job,
    const std::string& extension_id,
    std::unique_ptr<printing::PrintSettings> settings) {
  job_id_++;
  job->SetSource(printing::PrintJob::Source::EXTENSION, extension_id);

  absl::optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(base::UTF16ToUTF8(settings->device_name()));
  if (!printer) {
    int observer_count = 0;
    for (auto& observer : job->GetObserversForTesting()) {
      observer.OnFailed();
      observer_count++;
    }
    EXPECT_EQ(1, observer_count);
    return;
  }

  auto document = base::MakeRefCounted<printing::PrintedDocument>(
      std::move(settings), std::u16string(), 0);
  int observer_count = 0;
  for (auto& observer : job->GetObserversForTesting()) {
    observer.OnDocDone(job_id_, document.get());
    observer_count++;
  }
  EXPECT_EQ(1, observer_count);

  // Create a new CupsPrintJob.
  auto print_job = std::make_unique<ash::CupsPrintJob>(
      *printer, job_id_, base::UTF16ToUTF8(document->settings().title()),
      /*total_page_number=*/1, printing::PrintJob::Source::EXTENSION,
      extension_id, ash::PrintSettingsToProto(document->settings()));
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->StartPrintJob(print_job.get());
  std::string id = print_job->GetUniqueId();
  jobs_[std::move(id)] = std::move(print_job);
}

}  // namespace extensions

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_controller.h"

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"

namespace printing {

namespace {

void OnPrintSettingsApplied(scoped_refptr<PrintJob> print_job,
                            std::unique_ptr<MetafileSkia> pdf,
                            std::unique_ptr<PrinterQuery> query,
                            uint32_t page_count,
                            PrintJob::Source source,
                            const std::string& source_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_GT(page_count, 0U);

  std::u16string title = query->settings().title();
  print_job->Initialize(std::move(query), title, page_count);
  print_job->SetSource(source, source_id);
  print_job->document()->SetDocument(std::move(pdf));
  print_job->StartPrinting();
}

}  // namespace

class PendingJob;

// Keeps track of pending jobs and removes them from the storage once
// processed.
class PrintJobController::PendingJobStorage {
 public:
  PendingJobStorage() = default;
  ~PendingJobStorage() = default;

  PendingJobStorage(const PendingJobStorage&) = delete;
  PendingJobStorage& operator=(const PendingJobStorage&) = delete;

  void StartWatchingPrintJob(scoped_refptr<PrintJob> print_job,
                             PrintJobCreatedCallback callback);

  void DeletePendingJobPlease(PendingJob* pending_job);

 private:
  using PendingJobs =
      base::flat_set<std::unique_ptr<PendingJob>, base::UniquePtrComparator>;

  PendingJobs pending_jobs_;
};

// Observes the given `print_job` and invokes `callback` once the job signals
// OnDocDone() or OnFailed().
class PendingJob : public PrintJob::Observer {
 public:
  PendingJob(PrintJobController::PendingJobStorage* storage,
             scoped_refptr<PrintJob> print_job,
             PrintJobController::PrintJobCreatedCallback callback);
  ~PendingJob() override;

  PendingJob(const PendingJob&) = delete;
  PendingJob& operator=(const PendingJob&) = delete;

  void OnDocDone(int job_id, PrintedDocument* document) override;
  void OnFailed() override;

 private:
  // `storage_` owns `this`.
  const raw_ref<PrintJobController::PendingJobStorage> storage_;

  scoped_refptr<PrintJob> print_job_;
  PrintJobController::PrintJobCreatedCallback callback_;
};

void PrintJobController::PendingJobStorage::StartWatchingPrintJob(
    scoped_refptr<PrintJob> print_job,
    PrintJobCreatedCallback callback) {
  auto pending_job = std::make_unique<PendingJob>(this, std::move(print_job),
                                                  std::move(callback));
  pending_jobs_.insert(std::move(pending_job));
}

void PrintJobController::PendingJobStorage::DeletePendingJobPlease(
    PendingJob* pending_job) {
  pending_jobs_.erase(pending_job);
}

PrintJobController::PrintJobController()
    : pending_job_storage_(std::make_unique<PendingJobStorage>()) {}

PrintJobController::~PrintJobController() = default;

void PrintJobController::StartWatchingPrintJob(
    scoped_refptr<PrintJob> print_job,
    PrintJobCreatedCallback callback) {
  pending_job_storage_->StartWatchingPrintJob(std::move(print_job),
                                              std::move(callback));
}

PendingJob::PendingJob(PrintJobController::PendingJobStorage* storage,
                       scoped_refptr<PrintJob> print_job,
                       PrintJobController::PrintJobCreatedCallback callback)
    : storage_(*storage),
      print_job_(std::move(print_job)),
      callback_(std::move(callback)) {
  print_job_->AddObserver(*this);
}

PendingJob::~PendingJob() {
  print_job_->RemoveObserver(*this);
}

void PendingJob::OnDocDone(int job_id, PrintedDocument* document) {
  auto document_ref = raw_ref<PrintedDocument>::from_ptr(document);
  std::move(callback_).Run(PrintJobCreatedInfo{job_id, document_ref});

  storage_->DeletePendingJobPlease(this);
}

void PendingJob::OnFailed() {
  std::move(callback_).Run(std::nullopt);

  storage_->DeletePendingJobPlease(this);
}

void PrintJobController::CreatePrintJob(std::unique_ptr<MetafileSkia> pdf,
                                        std::unique_ptr<PrintSettings> settings,
                                        uint32_t page_count,
                                        PrintJob::Source source,
                                        const std::string& source_id,
                                        PrintJobCreatedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto print_job =
      base::MakeRefCounted<PrintJob>(g_browser_process->print_job_manager());
  StartWatchingPrintJob(print_job, std::move(callback));

  auto query = PrinterQuery::Create(content::GlobalRenderFrameHostId());
  auto* query_ptr = query.get();

  query_ptr->SetSettingsFromPOD(
      std::move(settings),
      base::BindOnce(&OnPrintSettingsApplied, print_job, std::move(pdf),
                     std::move(query), page_count, source, source_id));
}

}  // namespace printing

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/print_job_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"

namespace extensions {

namespace {

using PrinterQueryCallback =
    base::OnceCallback<void(std::unique_ptr<printing::PrinterQuery>)>;

// Send initialized PrinterQuery to UI thread.
void OnSettingsSetOnIOThread(std::unique_ptr<printing::PrinterQuery> query,
                             PrinterQueryCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(query)));
}

void CreateQueryOnIOThread(std::unique_ptr<printing::PrintSettings> settings,
                           PrinterQueryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto query = std::make_unique<printing::PrinterQuery>(
      content::ChildProcessHost::kInvalidUniqueID,
      content::ChildProcessHost::kInvalidUniqueID);
  auto* query_ptr = query.get();
  query_ptr->SetSettingsFromPOD(
      std::move(settings),
      base::BindOnce(&OnSettingsSetOnIOThread, std::move(query),
                     std::move(callback)));
}

}  // namespace

// This class lives on UI thread.
class PrintJobControllerImpl : public PrintJobController {
 public:
  explicit PrintJobControllerImpl(
      chromeos::CupsPrintJobManager* print_job_manager);
  ~PrintJobControllerImpl() override;

  // PrintJobController:
  void StartPrintJob(const std::string& extension_id,
                     std::unique_ptr<printing::MetafileSkia> metafile,
                     std::unique_ptr<printing::PrintSettings> settings,
                     StartPrintJobCallback callback) override;

  bool CancelPrintJob(const std::string& job_id) override;

  // Moves print job pointer to |print_jobs_map_| and resolves corresponding
  void OnPrintJobCreated(
      const std::string& extension_id,
      const std::string& job_id,
      base::WeakPtr<chromeos::CupsPrintJob> cups_job) override;

  // Removes print job pointer from |print_jobs_map_| as the job is finished.
  void OnPrintJobFinished(const std::string& job_id) override;

 private:
  struct JobState {
    JobState(scoped_refptr<printing::PrintJob> job,
             StartPrintJobCallback callback);
    JobState(JobState&&);
    JobState& operator=(JobState&&);
    ~JobState();

    scoped_refptr<printing::PrintJob> job;
    StartPrintJobCallback callback;
  };

  void StartPrinting(const std::string& extension_id,
                     std::unique_ptr<printing::MetafileSkia> metafile,
                     StartPrintJobCallback callback,
                     std::unique_ptr<printing::PrinterQuery> query);

  // Stores mapping from extension id to queue of pending jobs to resolve.
  // Placing a job state in the map means that we sent print job to the printing
  // pipeline and have been waiting for the response with created job id.
  // After that we could resolve a callback and move PrintJob to global map.
  // We need to store job pointers to keep the current scheduled print jobs
  // alive (as they're ref counted).
  base::flat_map<std::string, base::queue<JobState>> extension_pending_jobs_;

  // Stores mapping from job id to printing::PrintJob.
  // This is needed to hold PrintJob pointer and correct handle CancelJob()
  // requests.
  base::flat_map<std::string, scoped_refptr<printing::PrintJob>>
      print_jobs_map_;

  // Stores mapping from job id to chromeos::CupsPrintJob.
  base::flat_map<std::string, base::WeakPtr<chromeos::CupsPrintJob>>
      cups_print_jobs_map_;

  // PrintingAPIHandler (which owns PrintJobController) depends on
  // CupsPrintJobManagerFactory, so |print_job_manager_| outlives
  // PrintJobController.
  chromeos::CupsPrintJobManager* const print_job_manager_;

  base::WeakPtrFactory<PrintJobControllerImpl> weak_ptr_factory_{this};
};

PrintJobControllerImpl::JobState::JobState(
    scoped_refptr<printing::PrintJob> job,
    StartPrintJobCallback callback)
    : job(job), callback(std::move(callback)) {}

PrintJobControllerImpl::JobState::JobState(JobState&&) = default;

PrintJobControllerImpl::JobState& PrintJobControllerImpl::JobState::operator=(
    JobState&&) = default;

PrintJobControllerImpl::JobState::~JobState() = default;

PrintJobControllerImpl::PrintJobControllerImpl(
    chromeos::CupsPrintJobManager* print_job_manager)
    : print_job_manager_(print_job_manager) {}

PrintJobControllerImpl::~PrintJobControllerImpl() = default;

void PrintJobControllerImpl::StartPrintJob(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    std::unique_ptr<printing::PrintSettings> settings,
    StartPrintJobCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CreateQueryOnIOThread, std::move(settings),
          base::BindOnce(&PrintJobControllerImpl::StartPrinting,
                         weak_ptr_factory_.GetWeakPtr(), extension_id,
                         std::move(metafile), std::move(callback))));
}

void PrintJobControllerImpl::StartPrinting(
    const std::string& extension_id,
    std::unique_ptr<printing::MetafileSkia> metafile,
    StartPrintJobCallback callback,
    std::unique_ptr<printing::PrinterQuery> query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto job = base::MakeRefCounted<printing::PrintJob>();
  // Save in separate variable because |query| is moved.
  std::u16string title = query->settings().title();
  job->Initialize(std::move(query), title, /*page_count=*/1);
  job->SetSource(printing::PrintJob::Source::EXTENSION, extension_id);
  printing::PrintedDocument* document = job->document();
  document->SetDocument(std::move(metafile));
  // Save PrintJob scoped refptr and callback to resolve when print job is
  // created.
  extension_pending_jobs_[extension_id].emplace(job, std::move(callback));
  job->StartPrinting();
}

bool PrintJobControllerImpl::CancelPrintJob(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = cups_print_jobs_map_.find(job_id);
  if (it == cups_print_jobs_map_.end() || !it->second)
    return false;
  print_job_manager_->CancelPrintJob(it->second.get());
  return true;
}

void PrintJobControllerImpl::OnPrintJobCreated(
    const std::string& extension_id,
    const std::string& job_id,
    base::WeakPtr<chromeos::CupsPrintJob> cups_job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!cups_print_jobs_map_.contains(job_id));
  cups_print_jobs_map_[job_id] = cups_job;

  auto it = extension_pending_jobs_.find(extension_id);
  if (it == extension_pending_jobs_.end())
    return;
  auto& pending_jobs = it->second;
  if (!pending_jobs.empty()) {
    // We need to move corresponding scoped refptr of PrintJob from queue
    // of pending pointers to global map. We can't drop it as the print job is
    // not completed yet so we should not destruct it.
    print_jobs_map_[job_id] = pending_jobs.front().job;
    // The job is submitted to CUPS so we have to resolve the first callback
    // in the corresponding queue.
    auto callback = std::move(pending_jobs.front().callback);
    pending_jobs.pop();
    if (pending_jobs.empty())
      extension_pending_jobs_.erase(it);
    std::move(callback).Run(std::make_unique<std::string>(job_id));
  }
}

void PrintJobControllerImpl::OnPrintJobFinished(const std::string& job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  print_jobs_map_.erase(job_id);
  cups_print_jobs_map_.erase(job_id);
}

// static
std::unique_ptr<PrintJobController> PrintJobController::Create(
    chromeos::CupsPrintJobManager* print_job_manager) {
  return std::make_unique<PrintJobControllerImpl>(print_job_manager);
}

}  // namespace extensions

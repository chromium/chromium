// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/printed_document.h"
#include "printing/printing_context.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/printing/xps_features.h"
#include "printing/printed_page_win.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

void DocDoneNotificationCallback(PrintJob* print_job,
                                 int job_id,
                                 PrintedDocument* document) {
  print_job->OnDocDone(job_id, document);
}

void FailedNotificationCallback(PrintJob* print_job) {
  print_job->OnFailed();
}

}  // namespace

PrintJobWorker::PrintJobWorker(
    std::unique_ptr<PrintingContext::Delegate> printing_context_delegate,
    std::unique_ptr<PrintingContext> printing_context,
    PrintJob* print_job)
    : printing_context_delegate_(std::move(printing_context_delegate)),
      printing_context_(std::move(printing_context)),
      print_job_(print_job),
      thread_("Printing_Worker") {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PrintJobWorker::~PrintJobWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Stop();
}

bool PrintJobWorker::StartPrintingSanityCheck(
    const PrintedDocument* new_document) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (page_number_ != PageNumber::npos()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (!document_) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  if (document_.get() != new_document) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  return true;
}

std::u16string PrintJobWorker::GetDocumentName(
    const PrintedDocument* new_document) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::u16string document_name = SimplifyDocumentTitle(document_->name());
  if (document_name.empty()) {
    document_name = SimplifyDocumentTitle(
        l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE));
  }
  return document_name;
}

bool PrintJobWorker::SetupDocument(const std::u16string& document_name) {
  mojom::ResultCode result = printing_context_->NewDocument(document_name);
  switch (result) {
    case mojom::ResultCode::kSuccess:
      return true;
    case mojom::ResultCode::kCanceled:
      OnCancel();
      return false;
    default:
      OnFailure();
      return false;
  }
}

void PrintJobWorker::StartPrinting(PrintedDocument* new_document) {
  if (!StartPrintingSanityCheck(new_document))
    return;

  if (!SetupDocument(GetDocumentName(new_document))) {
    return;
  }

  // This will start a loop to wait for the page data.
  OnNewPage();
  // Don't touch this anymore since the instance could be destroyed. It happens
  // if all the pages are printed a one sweep and the client doesn't have a
  // handle to us anymore. There's a timing issue involved between the worker
  // thread and the UI thread. Take no chance.
}

void PrintJobWorker::OnDocumentChanged(PrintedDocument* new_document) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (page_number_ != PageNumber::npos()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  document_ = new_document;
}

void PrintJobWorker::PostWaitForPage() {
  // We need to wait for the page to be available.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorker::OnNewPage, weak_factory_.GetWeakPtr()),
      base::Milliseconds(500));
}

void PrintJobWorker::OnNewPage() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!document_)
    return;

  bool do_spool_document = true;
#if BUILDFLAG(IS_WIN)
  const bool source_is_pdf =
      !print_job_->document()->settings().is_modifiable();
  if (!ShouldPrintUsingXps(source_is_pdf)) {
    // Using the Windows GDI print API.
    if (!OnNewPageHelperGdi())
      return;

    do_spool_document = false;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (do_spool_document) {
    if (!document_->GetMetafile()) {
      PostWaitForPage();
      return;
    }
    if (!SpoolDocument())
      return;
  }

  OnDocumentDone();
  // Don't touch `this` anymore since the instance could be destroyed.
}

#if BUILDFLAG(IS_WIN)
bool PrintJobWorker::OnNewPageHelperGdi() {
  if (page_number_ == PageNumber::npos()) {
    // Find first page to print.
    int page_count = document_->page_count();
    if (!page_count) {
      // We still don't know how many pages the document contains.
      return false;
    }
    // We have enough information to initialize `page_number_`.
    page_number_.Init(document_->settings().ranges(), page_count);
  }

  while (true) {
    scoped_refptr<PrintedPage> page = document_->GetPage(page_number_.ToUint());
    if (!page) {
      PostWaitForPage();
      return false;
    }
    // The page is there, print it.
    if (!SpoolPage(page.get()))
      return false;
    ++page_number_;
    if (page_number_ == PageNumber::npos())
      break;
  }
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJobWorker::Cancel() {
  // This is the only function that can be called from any thread.
  printing_context_->Cancel();
  // Cannot touch any member variable since we don't know in which thread
  // context we run.
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void PrintJobWorker::CleanupAfterContentAnalysisDenial() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "Canceling job due to content analysis";
}
#endif

bool PrintJobWorker::IsRunning() const {
  return thread_.IsRunning();
}

bool PrintJobWorker::PostTask(const base::Location& from_here,
                              base::OnceClosure task) {
  return task_runner_ && task_runner_->PostTask(from_here, std::move(task));
}

void PrintJobWorker::StopSoon() {
  thread_.StopSoon();
}

void PrintJobWorker::Stop() {
  thread_.Stop();
}

bool PrintJobWorker::Start() {
  bool result = thread_.Start();
  task_runner_ = thread_.task_runner();
  return result;
}

void PrintJobWorker::CheckDocumentSpoolingComplete() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(page_number_, PageNumber::npos());
  // PrintJob must own this, because only PrintJob can send notifications.
  DCHECK(print_job_);
}

void PrintJobWorker::OnDocumentDone() {
  CheckDocumentSpoolingComplete();

  int job_id = printing_context_->job_id();
  if (printing_context_->DocumentDone() != mojom::ResultCode::kSuccess) {
    OnFailure();
    return;
  }

  FinishDocumentDone(job_id);
}

void PrintJobWorker::FinishDocumentDone(int job_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(document_);
  print_job_->PostTask(
      FROM_HERE, base::BindOnce(&DocDoneNotificationCallback,
                                base::RetainedRef(print_job_.get()), job_id,
                                base::RetainedRef(document_)));

  // Makes sure the variables are reinitialized.
  document_ = nullptr;
}

#if BUILDFLAG(IS_WIN)
bool PrintJobWorker::SpoolPage(PrintedPage* page) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(page_number_, PageNumber::npos());

  // Actual printing.
  if (document_->RenderPrintedPage(*page, printing_context_.get()) !=
      mojom::ResultCode::kSuccess) {
    OnFailure();
    return false;
  }

  // Signal everyone that the page is printed.
  DCHECK(print_job_);
  print_job_->PostTask(FROM_HERE,
                       base::BindOnce(&PrintJob::OnPageDone, print_job_,
                                      base::RetainedRef(page)));
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

bool PrintJobWorker::SpoolDocument() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  mojom::ResultCode result =
      document_->RenderPrintedDocument(printing_context_.get());
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Failure to render printed document - error "
                       << result;
    OnFailure();
    return false;
  }
  return true;
}

void PrintJobWorker::OnCancel() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(print_job_);

  print_job_->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJob::Cancel, base::RetainedRef(print_job_.get())));
}

void PrintJobWorker::OnFailure() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(print_job_);

  // We may loose our last reference by broadcasting the FAILED event.
  scoped_refptr<PrintJob> handle(print_job_.get());

  print_job_->PostTask(FROM_HERE,
                       base::BindOnce(&FailedNotificationCallback,
                                      base::RetainedRef(print_job_.get())));
  Cancel();

  // Makes sure the variables are reinitialized.
  document_ = nullptr;
  page_number_ = PageNumber::npos();
}

}  // namespace printing

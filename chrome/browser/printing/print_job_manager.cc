// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_manager.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/printed_document.h"

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
#include "base/win/windows_h_disallowed.h"
#endif  // defined(_WINDOWS_)

namespace printing {

PrintQueriesQueue::PrintQueriesQueue() = default;

PrintQueriesQueue::~PrintQueriesQueue() {
  base::AutoLock lock(lock_);
  queued_queries_.clear();
}

void PrintQueriesQueue::QueuePrinterQuery(std::unique_ptr<PrinterQuery> query) {
  base::AutoLock lock(lock_);
  DCHECK(query);
  DCHECK(query->is_valid());
  queued_queries_.push_back(std::move(query));
}

std::unique_ptr<PrinterQuery> PrintQueriesQueue::PopPrinterQuery(
    int document_cookie) {
  base::AutoLock lock(lock_);
  for (auto it = queued_queries_.begin(); it != queued_queries_.end(); ++it) {
    std::unique_ptr<PrinterQuery>& query = *it;
    if (query->cookie() != document_cookie)
      continue;

    std::unique_ptr<PrinterQuery> current_query = std::move(query);
    queued_queries_.erase(it);
    DCHECK(current_query->is_valid());
    return current_query;
  }
  return nullptr;
}

std::unique_ptr<PrinterQuery> PrintQueriesQueue::CreatePrinterQuery(
    content::GlobalRenderFrameHostId rfh_id) {
  return PrinterQuery::Create(rfh_id);
}

void PrintQueriesQueue::Shutdown() {
  {
    base::AutoLock lock(lock_);
    queued_queries_.clear();
  }
}

PrintJobManager::PrintJobManager() = default;

PrintJobManager::~PrintJobManager() = default;

base::CallbackListSubscription PrintJobManager::AddDocDoneCallback(
    DocDoneCallback callback) {
  return callback_list_.Add(callback);
}

scoped_refptr<PrintQueriesQueue> PrintJobManager::queue() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!queue_)
    queue_ = base::MakeRefCounted<PrintQueriesQueue>();
  return queue_;
}

void PrintJobManager::SetQueueForTest(scoped_refptr<PrintQueriesQueue> queue) {
  if (queue_)
    queue_->Shutdown();
  queue_ = queue;
}

void PrintJobManager::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;
  StopJobs(true);
  if (queue_) {
    queue_->Shutdown();
    queue_ = nullptr;
  }
}

void PrintJobManager::StopJobs(bool wait_for_finish) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Copy the array since it can be modified in transit.
  PrintJobs to_stop;
  to_stop.swap(current_jobs_);

  for (auto job = to_stop.begin(); job != to_stop.end(); ++job) {
    // Wait for two minutes for the print job to be spooled.
    if (wait_for_finish)
      (*job)->FlushJob(base::Minutes(2));
    (*job)->Stop();
  }
}

void PrintJobManager::OnStarted(PrintJob* print_job) {
  if (is_shutdown_)
    return;

  // Causes a AddRef().
  bool inserted = current_jobs_.insert(print_job).second;
  DCHECK(inserted);
}

void PrintJobManager::OnDocDone(PrintJob* print_job,
                                PrintedDocument* document,
                                int job_id) {
  if (is_shutdown_)
    return;

  callback_list_.Notify(print_job, document, job_id);
}

void PrintJobManager::OnJobDone(PrintJob* print_job) {
  if (is_shutdown_)
    return;

  size_t erased = current_jobs_.erase(print_job);
  DCHECK_EQ(1U, erased);
}

void PrintJobManager::OnFailed(PrintJob* print_job) {
  if (is_shutdown_)
    return;

  current_jobs_.erase(print_job);
}

}  // namespace printing

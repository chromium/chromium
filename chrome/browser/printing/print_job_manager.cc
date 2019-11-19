// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_manager.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "printing/printed_document.h"

namespace printing {

PrintQueriesQueue::PrintQueriesQueue() {
}

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
    int render_process_id,
    int render_frame_id) {
  return std::make_unique<PrinterQuery>(render_process_id, render_frame_id);
}

void PrintQueriesQueue::Shutdown() {
  PrinterQueries queries_to_stop;
  {
    base::AutoLock lock(lock_);
    queued_queries_.swap(queries_to_stop);
  }
  // Stop all pending queries, requests to generate print preview do not have
  // corresponding PrintJob, so any pending preview requests are not covered
  // by PrintJobManager::StopJobs and should be stopped explicitly.
  for (auto& query : queries_to_stop) {
    query->PostTask(
        FROM_HERE, base::BindOnce(&PrinterQuery::StopWorker, std::move(query)));
  }
}

PrintJobManager::PrintJobManager() {
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::NotificationService::AllSources());
}

PrintJobManager::~PrintJobManager() {
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
  registrar_.RemoveAll();
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
      (*job)->FlushJob(base::TimeDelta::FromMinutes(2));
    (*job)->Stop();
  }
}

void PrintJobManager::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);

  OnPrintJobEvent(content::Source<PrintJob>(source).ptr(),
                  *content::Details<JobEventDetails>(details).ptr());
}

void PrintJobManager::OnPrintJobEvent(
    PrintJob* print_job,
    const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::NEW_DOC: {
      // Causes a AddRef().
      bool inserted = current_jobs_.insert(print_job).second;
      DCHECK(inserted);
      break;
    }
    case JobEventDetails::JOB_DONE: {
      size_t erased = current_jobs_.erase(print_job);
      DCHECK_EQ(1U, erased);
      break;
    }
    case JobEventDetails::FAILED: {
      current_jobs_.erase(print_job);
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED:
    case JobEventDetails::DEFAULT_INIT_DONE:
#if defined(OS_WIN)
    case JobEventDetails::PAGE_DONE:
#endif
    case JobEventDetails::DOC_DONE:
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      // Don't care.
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

}  // namespace printing

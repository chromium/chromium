// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace content {
struct GlobalRenderFrameHostId;
}

namespace printing {

class PrintJob;
class PrintedDocument;
class PrinterQuery;

class PrintQueriesQueue : public base::RefCountedThreadSafe<PrintQueriesQueue> {
 public:
  PrintQueriesQueue();

  PrintQueriesQueue(const PrintQueriesQueue&) = delete;
  PrintQueriesQueue& operator=(const PrintQueriesQueue&) = delete;

  // Queues a semi-initialized worker thread. Can be called from any thread.
  // Current use case is queuing from the I/O thread.
  // TODO(maruel):  Have them vanish after a timeout (~5 minutes?)
  void QueuePrinterQuery(std::unique_ptr<PrinterQuery> query);

  // Pops a queued PrinterQuery object that was previously queued.  Returns
  // nullptr if there is no query matching `document_cookie`.
  // Can be called from any thread.
  std::unique_ptr<PrinterQuery> PopPrinterQuery(int document_cookie);

  // Creates new query. Virtual so that tests can override it.
  virtual std::unique_ptr<PrinterQuery> CreatePrinterQuery(
      content::GlobalRenderFrameHostId rfh_id);

  void Shutdown();

 protected:
  // Protected for unit tests.
  virtual ~PrintQueriesQueue();

 private:
  friend class base::RefCountedThreadSafe<PrintQueriesQueue>;
  using PrinterQueries = std::vector<std::unique_ptr<PrinterQuery>>;

  // Used to serialize access to `queued_queries_`.
  base::Lock lock_;

  PrinterQueries queued_queries_;
};

class PrintJobManager {
 public:
  PrintJobManager();

  PrintJobManager(const PrintJobManager&) = delete;
  PrintJobManager& operator=(const PrintJobManager&) = delete;

  ~PrintJobManager();

  using DocDoneCallbackList = base::RepeatingCallbackList<
      void(PrintJob* job, PrintedDocument* document, int job_id)>;
  using DocDoneCallback = DocDoneCallbackList::CallbackType;

  // Call this method to be informed of DocDone events for all PrintJob
  // instances.
  // NOTE: If you need to be invoked of such events only for a specific
  // instance, you should instead observe that instance via PrintJob::Observer.
  base::CallbackListSubscription AddDocDoneCallback(DocDoneCallback callback);

  // On browser quit, we should wait to have the print job finished.
  void Shutdown();

  // Returns queries queue. Never returns NULL. Must be called on Browser UI
  // Thread. Reference could be stored and used from any thread.
  scoped_refptr<PrintQueriesQueue> queue();

  // Sets the queries queue for testing.
  void SetQueueForTest(scoped_refptr<PrintQueriesQueue> queue);

  // Invoked by PrintJob when printing is started.
  void OnStarted(PrintJob* print_job);

  // Invoked by PrintJob when a document is done.
  void OnDocDone(PrintJob* print_job, PrintedDocument* document, int job_id);

  // Invoked by PrintJob when the job is done.
  void OnJobDone(PrintJob* print_job);

  // Invoked by PrintJob when the job has failed.
  void OnFailed(PrintJob* print_job);

 private:
  using PrintJobs = std::set<scoped_refptr<PrintJob>>;

  // Stops all printing jobs. If wait_for_finish is true, tries to give jobs
  // a chance to complete before stopping them.
  void StopJobs(bool wait_for_finish);

  // Current print jobs that are active.
  PrintJobs current_jobs_;

  scoped_refptr<PrintQueriesQueue> queue_;

  DocDoneCallbackList callback_list_;

  bool is_shutdown_ = false;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_

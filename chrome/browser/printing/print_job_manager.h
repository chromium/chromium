// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace printing {

class JobEventDetails;
class PrintJob;
class PrinterQuery;

class PrintQueriesQueue : public base::RefCountedThreadSafe<PrintQueriesQueue> {
 public:
  PrintQueriesQueue();

  // Queues a semi-initialized worker thread. Can be called from any thread.
  // Current use case is queuing from the I/O thread.
  // TODO(maruel):  Have them vanish after a timeout (~5 minutes?)
  void QueuePrinterQuery(std::unique_ptr<PrinterQuery> query);

  // Pops a queued PrinterQuery object that was previously queued or creates
  // a new one. Can be called from any thread.
  std::unique_ptr<PrinterQuery> PopPrinterQuery(int document_cookie);

  // Creates new query. Virtual so that tests can override it.
  virtual std::unique_ptr<PrinterQuery> CreatePrinterQuery(
      int render_process_id,
      int render_frame_id);

  void Shutdown();

 protected:
  // Protected for unit tests.
  virtual ~PrintQueriesQueue();

 private:
  friend class base::RefCountedThreadSafe<PrintQueriesQueue>;
  using PrinterQueries = std::vector<std::unique_ptr<PrinterQuery>>;

  // Used to serialize access to |queued_queries_|.
  base::Lock lock_;

  PrinterQueries queued_queries_;

  DISALLOW_COPY_AND_ASSIGN(PrintQueriesQueue);
};

class PrintJobManager : public content::NotificationObserver {
 public:
  PrintJobManager();
  ~PrintJobManager() override;

  // On browser quit, we should wait to have the print job finished.
  void Shutdown();

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns queries queue. Never returns NULL. Must be called on Browser UI
  // Thread. Reference could be stored and used from any thread.
  scoped_refptr<PrintQueriesQueue> queue();

  // Sets the queries queue for testing.
  void SetQueueForTest(scoped_refptr<PrintQueriesQueue> queue);

 private:
  using PrintJobs = std::set<scoped_refptr<PrintJob>>;

  // Processes a NOTIFY_PRINT_JOB_EVENT notification.
  void OnPrintJobEvent(PrintJob* print_job,
                       const JobEventDetails& event_details);

  // Stops all printing jobs. If wait_for_finish is true, tries to give jobs
  // a chance to complete before stopping them.
  void StopJobs(bool wait_for_finish);

  content::NotificationRegistrar registrar_;

  // Current print jobs that are active.
  PrintJobs current_jobs_;

  scoped_refptr<PrintQueriesQueue> queue_;

  bool is_shutdown_ = false;

  DISALLOW_COPY_AND_ASSIGN(PrintJobManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_

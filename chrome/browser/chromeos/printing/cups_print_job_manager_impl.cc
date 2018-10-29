// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

#include <cups/cups.h>
#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "printing/backend/cups_connection.h"
#include "printing/printed_document.h"

namespace {

// The rate at which we will poll CUPS for print job updates.
constexpr base::TimeDelta kPollRate = base::TimeDelta::FromMilliseconds(1000);

// Threshold for giving up on communicating with CUPS.
const int kRetryMax = 6;

// job state reason values
const char kJobCompletedWithErrors[] = "job-completed-with-errors";

using State = chromeos::CupsPrintJob::State;
using ErrorCode = chromeos::CupsPrintJob::ErrorCode;

using PrinterReason = printing::PrinterStatus::PrinterReason;

// Enumeration of print job results for histograms.  Do not modify!
enum JobResultForHistogram {
  UNKNOWN = 0,         // unidentified result
  FINISHED = 1,        // successful completion of job
  TIMEOUT_CANCEL = 2,  // cancelled due to timeout
  PRINTER_CANCEL = 3,  // cancelled by printer
  LOST = 4,            // final state never received
  FILTER_FAILED = 5,   // filter failed
  RESULT_MAX
};

// Container for results from CUPS queries.
struct QueryResult {
  QueryResult() = default;
  QueryResult(const QueryResult& other) = default;
  ~QueryResult() = default;

  bool success;
  std::vector<::printing::QueueStatus> queues;
};

// Returns the appropriate JobResultForHistogram for a given |state|.  Only
// FINISHED and PRINTER_CANCEL are derived from CupsPrintJob::State.
JobResultForHistogram ResultForHistogram(State state) {
  switch (state) {
    case State::STATE_DOCUMENT_DONE:
      return FINISHED;
    case State::STATE_CANCELLED:
      return PRINTER_CANCEL;
    default:
      break;
  }

  return UNKNOWN;
}

void RecordJobResult(JobResultForHistogram result) {
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.JobResult", result, RESULT_MAX);
}

// Returns the equivalient CupsPrintJob#State from a CupsJob#JobState.
chromeos::CupsPrintJob::State ConvertState(printing::CupsJob::JobState state) {
  switch (state) {
    case printing::CupsJob::PENDING:
      return State::STATE_WAITING;
    case printing::CupsJob::HELD:
      return State::STATE_SUSPENDED;
    case printing::CupsJob::PROCESSING:
      return State::STATE_STARTED;
    case printing::CupsJob::CANCELED:
      return State::STATE_CANCELLED;
    case printing::CupsJob::COMPLETED:
      return State::STATE_DOCUMENT_DONE;
    case printing::CupsJob::STOPPED:
      return State::STATE_SUSPENDED;
    case printing::CupsJob::ABORTED:
      return State::STATE_ERROR;
    case printing::CupsJob::UNKNOWN:
      break;
  }

  NOTREACHED();

  return State::STATE_NONE;
}

// Returns true if |job|.state_reasons contains |reason|
bool JobContainsReason(const ::printing::CupsJob& job,
                       base::StringPiece reason) {
  return base::ContainsValue(job.state_reasons, reason);
}

// Extracts an ErrorCode from PrinterStatus#reasons.  Returns NO_ERROR if there
// are no reasons which indicate an error.
chromeos::CupsPrintJob::ErrorCode ErrorCodeFromReasons(
    const printing::PrinterStatus& printer_status) {
  for (const auto& reason : printer_status.reasons) {
    switch (reason.reason) {
      case PrinterReason::MEDIA_JAM:
      case PrinterReason::MEDIA_EMPTY:
      case PrinterReason::MEDIA_NEEDED:
      case PrinterReason::MEDIA_LOW:
        return chromeos::CupsPrintJob::ErrorCode::PAPER_JAM;
      case PrinterReason::TONER_EMPTY:
      case PrinterReason::TONER_LOW:
        return chromeos::CupsPrintJob::ErrorCode::OUT_OF_INK;
      default:
        break;
    }
  }
  return chromeos::CupsPrintJob::ErrorCode::NO_ERROR;
}

// Update the current printed page.  Returns true of the page has been updated.
bool UpdateCurrentPage(const printing::CupsJob& job,
                       chromeos::CupsPrintJob* print_job) {
  bool pages_updated = false;
  if (job.current_pages <= 0) {
    print_job->set_printed_page_number(0);
    print_job->set_state(State::STATE_STARTED);
  } else {
    pages_updated = job.current_pages != print_job->printed_page_number();
    print_job->set_printed_page_number(job.current_pages);
    print_job->set_state(State::STATE_PAGE_DONE);
  }

  return pages_updated;
}

// Updates the state of a print job based on |printer_status| and |job|.
// Returns true if observers need to be notified of an update.
bool UpdatePrintJob(const ::printing::PrinterStatus& printer_status,
                    const ::printing::CupsJob& job,
                    chromeos::CupsPrintJob* print_job) {
  DCHECK_EQ(job.id, print_job->job_id());

  State old_state = print_job->state();

  bool pages_updated = false;
  switch (job.state) {
    case ::printing::CupsJob::PROCESSING:
      pages_updated = UpdateCurrentPage(job, print_job);
      break;
    case ::printing::CupsJob::COMPLETED:
      DCHECK_GE(job.current_pages, print_job->total_page_number());
      print_job->set_state(State::STATE_DOCUMENT_DONE);
      break;
    case ::printing::CupsJob::STOPPED:
      // If cups job STOPPED but with filter failure, treat as ERROR
      if (JobContainsReason(job, kJobCompletedWithErrors)) {
        print_job->set_error_code(
            chromeos::CupsPrintJob::ErrorCode::FILTER_FAILED);
        print_job->set_state(chromeos::CupsPrintJob::State::STATE_ERROR);
      } else {
        print_job->set_state(ConvertState(job.state));
      }
      break;
    case ::printing::CupsJob::ABORTED:
    case ::printing::CupsJob::CANCELED:
      print_job->set_error_code(ErrorCodeFromReasons(printer_status));
      FALLTHROUGH;
    default:
      print_job->set_state(ConvertState(job.state));
      break;
  }

  return print_job->state() != old_state || pages_updated;
}

}  // namespace

namespace chromeos {

// A wrapper around the CUPS connection to ensure that it's always accessed on
// the same sequence.
class CupsWrapper {
 public:
  CupsWrapper() : cups_connection_(GURL(), HTTP_ENCRYPT_NEVER, false) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~CupsWrapper() = default;

  // Query CUPS for the current jobs for the given |printer_ids|.  Writes result
  // to |result|.
  void QueryCups(const std::vector<std::string>& printer_ids,
                 QueryResult* result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);

    result->success = cups_connection_.GetJobs(printer_ids, &result->queues);
  }

  // Cancel the print job on the blocking thread.
  void CancelJobImpl(const std::string& printer_id, const int job_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);

    std::unique_ptr<::printing::CupsPrinter> printer =
        cups_connection_.GetPrinter(printer_id);
    if (!printer) {
      LOG(WARNING) << "Printer not found: " << printer_id;
      return;
    }

    if (!printer->CancelJob(job_id)) {
      // This is not expected to fail but log it if it does.
      LOG(WARNING) << "Cancelling job failed.  Job may be stuck in queue.";
    }
  }

 private:
  ::printing::CupsConnection cups_connection_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CupsWrapper);
};

class CupsPrintJobManagerImpl : public CupsPrintJobManager,
                                public content::NotificationObserver {
 public:
  explicit CupsPrintJobManagerImpl(Profile* profile)
      : CupsPrintJobManager(profile),
        query_runner_(base::CreateSequencedTaskRunnerWithTraits(
            base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                             base::MayBlock(),
                             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN))),
        cups_wrapper_(new CupsWrapper(),
                      base::OnTaskRunnerDeleter(query_runner_)),
        weak_ptr_factory_(this) {
    timer_.SetTaskRunner(base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::UI}));
    registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                   content::NotificationService::AllSources());
  }

  ~CupsPrintJobManagerImpl() override = default;

  // CupsPrintJobManager overrides:
  // Must be run from the UI thread.
  void CancelPrintJob(CupsPrintJob* job) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    job->set_state(CupsPrintJob::State::STATE_CANCELLED);
    NotifyJobCanceled(job->GetWeakPtr());
    // Ideally we should wait for IPP response.
    FinishPrintJob(job);
  }

  bool SuspendPrintJob(CupsPrintJob* job) override {
    NOTREACHED() << "Pause printer is not implemented";
    return false;
  }

  bool ResumePrintJob(CupsPrintJob* job) override {
    NOTREACHED() << "Resume printer is not implemented";
    return false;
  }

  // NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);

    content::Details<::printing::JobEventDetails> job_details(details);

    // DOC_DONE occurs after the print job has been successfully sent to the
    // spooler which is when we begin tracking the print queue.
    if (job_details->type() == ::printing::JobEventDetails::DOC_DONE) {
      const ::printing::PrintedDocument* document = job_details->document();
      DCHECK(document);
      CreatePrintJob(base::UTF16ToUTF8(document->settings().device_name()),
                     base::UTF16ToUTF8(document->settings().title()),
                     job_details->job_id(), document->page_count());
    }
  }

 private:
  // Begin monitoring a print job for a given |printer_name| with the given
  // |title| with the pages |total_page_number|.
  bool CreatePrintJob(const std::string& printer_name,
                      const std::string& title,
                      int job_id,
                      int total_page_number) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto printer = SyncedPrintersManagerFactory::GetForBrowserContext(profile_)
                       ->GetPrinter(printer_name);
    if (!printer) {
      LOG(WARNING)
          << "Printer was removed while job was in progress.  It cannot "
             "be tracked";
      return false;
    }

    // Records the number of jobs we're currently tracking when a new job is
    // started.  This is equivalent to print queue size in the current
    // implementation.
    UMA_HISTOGRAM_EXACT_LINEAR("Printing.CUPS.PrintJobsQueued", jobs_.size(),
                               20);

    // Create a new print job.
    auto cpj = std::make_unique<CupsPrintJob>(*printer, job_id, title,
                                              total_page_number);
    std::string key = cpj->GetUniqueId();
    jobs_[key] = std::move(cpj);

    CupsPrintJob* job = jobs_[key].get();
    NotifyJobCreated(job->GetWeakPtr());

    // Always start jobs in the waiting state.
    job->set_state(CupsPrintJob::State::STATE_WAITING);
    NotifyJobUpdated(job->GetWeakPtr());

    // Run a query now.
    base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
        ->PostTask(FROM_HERE, base::Bind(&CupsPrintJobManagerImpl::PostQuery,
                                         weak_ptr_factory_.GetWeakPtr()));
    // Start the timer for ongoing queries.
    ScheduleQuery();

    return true;
  }

  void FinishPrintJob(CupsPrintJob* job) {
    // Copy job_id and printer_id.  |job| is about to be freed.
    const int job_id = job->job_id();
    const std::string printer_id = job->printer().id();

    // Stop montioring jobs after we cancel them.  The user no longer cares.
    jobs_.erase(job->GetUniqueId());

    query_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CupsWrapper::CancelJobImpl,
                                  base::Unretained(cups_wrapper_.get()),
                                  printer_id, job_id));
  }

  // Schedule a query of CUPS for print job status with a delay of |delay|.
  void ScheduleQuery(int attempt_count = 1) {
    timer_.Start(FROM_HERE, kPollRate * attempt_count,
                 base::Bind(&CupsPrintJobManagerImpl::PostQuery,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Schedule the CUPS query off the UI thread. Posts results back to UI thread
  // to UpdateJobs.
  void PostQuery() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // The set of active printers is expected to be small.
    std::set<std::string> printer_ids;
    for (const auto& entry : jobs_) {
      printer_ids.insert(entry.second->printer().id());
    }
    std::vector<std::string> ids{printer_ids.begin(), printer_ids.end()};

    auto result = std::make_unique<QueryResult>();
    QueryResult* result_ptr = result.get();
    // Runs a query on |query_runner_| which will rejoin this sequnece on
    // completion.
    query_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&CupsWrapper::QueryCups,
                   base::Unretained(cups_wrapper_.get()), ids, result_ptr),
        base::Bind(&CupsPrintJobManagerImpl::UpdateJobs,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&result)));
  }

  // Process jobs from CUPS and perform notifications.
  // Use job information to update local job states.  Previously completed jobs
  // could be in |jobs| but those are ignored as we will not emit updates for
  // them after they are completed.
  void UpdateJobs(std::unique_ptr<QueryResult> result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // If the query failed, either retry or purge.
    if (!result->success) {
      retry_count_++;
      LOG(WARNING) << "Failed to query CUPS for queue status.  Schedule retry ("
                   << retry_count_ << ")";
      if (retry_count_ > kRetryMax) {
        LOG(ERROR) << "CUPS is unreachable.  Giving up on all jobs.";
        timer_.Stop();
        PurgeJobs();
      } else {
        // Backoff the polling frequency. Give CUPS a chance to recover.
        DCHECK_GE(1, retry_count_);
        ScheduleQuery(retry_count_);
      }
      return;
    }

    // A query has completed.  Reset retry counter.
    retry_count_ = 0;

    std::vector<std::string> active_jobs;
    for (const auto& queue : result->queues) {
      for (auto& job : queue.jobs) {
        std::string key = CupsPrintJob::CreateUniqueId(job.printer_id, job.id);
        const auto& entry = jobs_.find(key);
        if (entry == jobs_.end())
          continue;

        CupsPrintJob* print_job = entry->second.get();

        if (UpdatePrintJob(queue.printer_status, job, print_job)) {
          // The state of the job changed, notify observers.
          NotifyJobStateUpdate(print_job->GetWeakPtr());
        }

        if (print_job->PipelineDead()) {
          RecordJobResult(FILTER_FAILED);
          FinishPrintJob(print_job);
        } else if (print_job->IsJobFinished()) {
          // Cleanup completed jobs.
          VLOG(1) << "Removing Job " << print_job->document_title();
          RecordJobResult(ResultForHistogram(print_job->state()));
          jobs_.erase(entry);
        } else {
          active_jobs.push_back(key);
        }
      }
    }

    if (active_jobs.empty()) {
      // CUPS has stopped reporting jobs.  Stop polling.
      timer_.Stop();
      if (!jobs_.empty()) {
        // We're tracking jobs that we didn't receive an update for.  Something
        // bad has happened.
        LOG(ERROR) << "Lost track of (" << jobs_.size() << ") jobs";
        PurgeJobs();
      }
    }
  }

  // Mark remaining jobs as errors and remove active jobs.
  void PurgeJobs() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    for (const auto& entry : jobs_) {
      // Declare all lost jobs errors.
      RecordJobResult(LOST);
      CupsPrintJob* job = entry.second.get();
      job->set_state(CupsPrintJob::State::STATE_ERROR);
      NotifyJobStateUpdate(job->GetWeakPtr());
    }

    jobs_.clear();
  }

  // Notify observers that a state update has occured for |job|.
  void NotifyJobStateUpdate(base::WeakPtr<CupsPrintJob> job) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (!job)
      return;

    switch (job->state()) {
      case State::STATE_NONE:
        // State does not require notification.
        break;
      case State::STATE_WAITING:
        NotifyJobUpdated(job);
        break;
      case State::STATE_STARTED:
        NotifyJobStarted(job);
        break;
      case State::STATE_PAGE_DONE:
        NotifyJobUpdated(job);
        break;
      case State::STATE_RESUMED:
        NotifyJobResumed(job);
        break;
      case State::STATE_SUSPENDED:
        NotifyJobSuspended(job);
        break;
      case State::STATE_CANCELLED:
        NotifyJobCanceled(job);
        break;
      case State::STATE_ERROR:
        NotifyJobError(job);
        break;
      case State::STATE_DOCUMENT_DONE:
        NotifyJobDone(job);
        break;
    }
  }

  // Ongoing print jobs.
  std::map<std::string, std::unique_ptr<CupsPrintJob>> jobs_;

  // Records the number of consecutive times the GetJobs query has failed.
  int retry_count_ = 0;

  base::RepeatingTimer timer_;
  content::NotificationRegistrar registrar_;
  // Task runner for queries to CUPS.
  scoped_refptr<base::SequencedTaskRunner> query_runner_;
  std::unique_ptr<CupsWrapper, base::OnTaskRunnerDeleter> cups_wrapper_;
  base::WeakPtrFactory<CupsPrintJobManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManagerImpl);
};

// static
CupsPrintJobManager* CupsPrintJobManager::CreateInstance(Profile* profile) {
  return new CupsPrintJobManagerImpl(profile);
}

}  // namespace chromeos

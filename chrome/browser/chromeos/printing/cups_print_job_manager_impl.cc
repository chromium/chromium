// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"

#include <cups/cups.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/printing/history/print_job_info_proto_conversions.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The rate at which we will poll CUPS for print job updates.
constexpr base::TimeDelta kPollRate = base::TimeDelta::FromMilliseconds(1000);

// Threshold for giving up on communicating with CUPS.
const int kRetryMax = 6;

// job state reason values
const char kJobCompletedWithErrors[] = "job-completed-with-errors";

using State = chromeos::CupsPrintJob::State;
using PrinterErrorCode = chromeos::PrinterErrorCode;

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
State ConvertState(printing::CupsJob::JobState state) {
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
      return State::STATE_FAILED;
    case printing::CupsJob::UNKNOWN:
      break;
  }

  NOTREACHED();

  return State::STATE_NONE;
}

// Returns true if |job|.state_reasons contains |reason|
bool JobContainsReason(const ::printing::CupsJob& job,
                       base::StringPiece reason) {
  return base::Contains(job.state_reasons, reason);
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
      if (chromeos::PrinterErrorCodeFromPrinterStatusReasons(printer_status) !=
          PrinterErrorCode::NO_ERROR) {
        print_job->set_error_code(
            chromeos::PrinterErrorCodeFromPrinterStatusReasons(printer_status));
        print_job->set_state(State::STATE_ERROR);
      } else {
        print_job->set_state(State::STATE_STARTED);
      }
      break;
    case ::printing::CupsJob::COMPLETED:
      DCHECK_GE(job.current_pages, print_job->total_page_number());
      print_job->set_state(State::STATE_DOCUMENT_DONE);
      break;
    case ::printing::CupsJob::STOPPED:
      // If cups job STOPPED but with filter failure, treat as ERROR
      if (JobContainsReason(job, kJobCompletedWithErrors)) {
        print_job->set_error_code(PrinterErrorCode::FILTER_FAILED);
        print_job->set_state(State::STATE_FAILED);
      } else {
        print_job->set_state(ConvertState(job.state));
      }
      break;
    case ::printing::CupsJob::ABORTED:
    case ::printing::CupsJob::CANCELED:
      print_job->set_error_code(
          chromeos::PrinterErrorCodeFromPrinterStatusReasons(printer_status));
      FALLTHROUGH;
    default:
      print_job->set_state(ConvertState(job.state));
      break;
  }

  return print_job->state() != old_state || pages_updated;
}

}  // namespace

namespace chromeos {

class CupsPrintJobManagerImpl : public CupsPrintJobManager,
                                public content::NotificationObserver {
 public:
  explicit CupsPrintJobManagerImpl(Profile* profile)
      : CupsPrintJobManager(profile),
        weak_ptr_factory_(this) {
    timer_.SetTaskRunner(
        base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}));
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
    content::Source<::printing::PrintJob> job(source);

    // DOC_DONE occurs after the print job has been successfully sent to the
    // spooler which is when we begin tracking the print queue.
    if (job_details->type() == ::printing::JobEventDetails::DOC_DONE) {
      const ::printing::PrintedDocument* document = job_details->document();
      DCHECK(document);
      base::string16 title =
          ::printing::SimplifyDocumentTitle(document->name());
      if (title.empty()) {
        title = ::printing::SimplifyDocumentTitle(
            l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE));
      }
      CreatePrintJob(base::UTF16ToUTF8(document->settings().device_name()),
                     base::UTF16ToUTF8(title), job_details->job_id(),
                     document->page_count(), job->source(), job->source_id(),
                     PrintSettingsToProto(document->settings()));
    }
  }

 private:
  // Begin monitoring a print job for a given |printer_name| with the given
  // |title| with the pages |total_page_number|.
  bool CreatePrintJob(const std::string& printer_name,
                      const std::string& title,
                      int job_id,
                      int total_page_number,
                      ::printing::PrintJob::Source source,
                      const std::string& source_id,
                      const printing::proto::PrintSettings& settings) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    Profile* profile = ProfileManager::GetPrimaryUserProfile();
    if (!profile) {
      LOG(WARNING) << "Cannot find printer without a valid profile.";
      return false;
    }

    auto* manager = CupsPrintersManagerFactory::GetForBrowserContext(profile);
    if (!manager) {
      LOG(WARNING)
          << "CupsPrintersManager could not be found for the current profile.";
      return false;
    }

    base::Optional<Printer> printer = manager->GetPrinter(printer_name);
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
                                              total_page_number, source,
                                              source_id, settings);
    std::string key = cpj->GetUniqueId();
    jobs_[key] = std::move(cpj);

    CupsPrintJob* job = jobs_[key].get();
    NotifyJobCreated(job->GetWeakPtr());

    // Always start jobs in the waiting state.
    job->set_state(CupsPrintJob::State::STATE_WAITING);
    NotifyJobUpdated(job->GetWeakPtr());

    // Run a query now.
    base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&CupsPrintJobManagerImpl::PostQuery,
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

    cups_wrapper_.CancelJob(printer_id, job_id);
  }

  // Schedule a query of CUPS for print job status with a delay of |delay|.
  void ScheduleQuery(int attempt_count = 1) {
    timer_.Start(FROM_HERE, kPollRate * attempt_count,
                 base::BindRepeating(&CupsPrintJobManagerImpl::PostQuery,
                                     weak_ptr_factory_.GetWeakPtr()));
  }

  void PostQuery() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // The set of active printers is expected to be small.
    std::set<std::string> printer_ids;
    for (const auto& entry : jobs_) {
      printer_ids.insert(entry.second->printer().id());
    }
    std::vector<std::string> ids{printer_ids.begin(), printer_ids.end()};

    cups_wrapper_.QueryCupsPrintJobs(
        ids, base::BindOnce(&CupsPrintJobManagerImpl::UpdateJobs,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Process jobs from CUPS and perform notifications.
  // Use job information to update local job states.  Previously completed jobs
  // could be in |jobs| but those are ignored as we will not emit updates for
  // them after they are completed.
  void UpdateJobs(std::unique_ptr<CupsWrapper::QueryResult> result) {
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

        if (print_job->IsExpired()) {
          // Job needs to be forcibly cancelled.
          RecordJobResult(TIMEOUT_CANCEL);
          FinishPrintJob(print_job);
          // Beware, print_job was removed from jobs_ and
          // deleted.
        } else if (print_job->PipelineDead()) {
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
      job->set_state(CupsPrintJob::State::STATE_FAILED);
      NotifyJobStateUpdate(job->GetWeakPtr());
    }

    jobs_.clear();
  }

  // Notify observers that a state update has occurred for |job|.
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
      case State::STATE_FAILED:
        NotifyJobFailed(job);
        break;
      case State::STATE_DOCUMENT_DONE:
        NotifyJobDone(job);
        break;
      case State::STATE_ERROR:
        NotifyJobUpdated(job);
        break;
    }
  }

  // Ongoing print jobs.
  std::map<std::string, std::unique_ptr<CupsPrintJob>> jobs_;

  // Records the number of consecutive times the GetJobs query has failed.
  int retry_count_ = 0;

  base::RepeatingTimer timer_;
  content::NotificationRegistrar registrar_;
  CupsWrapper cups_wrapper_;
  base::WeakPtrFactory<CupsPrintJobManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManagerImpl);
};

// static
CupsPrintJobManager* CupsPrintJobManager::CreateInstance(Profile* profile) {
  return new CupsPrintJobManagerImpl(profile);
}

}  // namespace chromeos

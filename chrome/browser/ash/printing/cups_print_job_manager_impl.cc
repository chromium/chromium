// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/cups.h>

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_utils.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/ash/printing/history/print_job_info_proto_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "chromeos/printing/printing_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using ::chromeos::CupsWrapper;
using StatusReason = crosapi::mojom::StatusReason::Reason;

// The rate at which we will poll CUPS for print job updates.
constexpr base::TimeDelta kPollRate = base::Milliseconds(1000);

// Threshold for giving up on communicating with CUPS.
const int kRetryMax = 6;

// Enumeration of print job results for histograms.  Do not modify!
enum JobResultForHistogram {
  UNKNOWN = 0,              // unidentified result
  FINISHED = 1,             // successful completion of job
  TIMEOUT_CANCEL = 2,       // cancelled due to timeout
  PRINTER_CANCEL = 3,       // cancelled by printer
  LOST = 4,                 // final state never received
  FILTER_FAILED = 5,        // filter failed
  CLIENT_UNAUTHORIZED = 6,  // cancelled due to client unauthorized
  RESULT_MAX
};

// Holds the print job data for recording to metrics upon print job completion.
struct PrinterMetrics {
  bool printer_manually_selected;
  std::optional<StatusReason> printer_status_reason;
};

// Returns the appropriate JobResultForHistogram for a given |state|.  Only
// FINISHED and PRINTER_CANCEL are derived from CupsPrintJob::State.
JobResultForHistogram ResultForHistogram(CupsPrintJob::State state) {
  switch (state) {
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      return FINISHED;
    case CupsPrintJob::State::STATE_CANCELLED:
      return PRINTER_CANCEL;
    default:
      break;
  }

  return UNKNOWN;
}

void RecordJobResult(JobResultForHistogram result,
                     bool affected_by_ipp_usb_migration) {
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.JobResult", result, RESULT_MAX);
  if (affected_by_ipp_usb_migration) {
    base::UmaHistogramEnumeration(
        "Printing.CUPS.JobResultForUsbPrintersWithIppAndPpd", result,
        RESULT_MAX);
  }
}

}  // namespace

class CupsPrintJobManagerImpl : public CupsPrintJobManager {
 public:
  explicit CupsPrintJobManagerImpl(Profile* profile)
      : CupsPrintJobManager(profile),
        cups_wrapper_(CupsWrapper::Create()),
        weak_ptr_factory_(this) {
    // NOTE: base::Unretained(this) is safe here because this object owns
    // |subscription_| and the callback won't be invoked after |subscription_|
    // is destroyed.
    subscription_ = g_browser_process->print_job_manager()->AddDocDoneCallback(
        base::BindRepeating(&CupsPrintJobManagerImpl::OnDocDone,
                            base::Unretained(this)));
    timer_.SetTaskRunner(content::GetUIThreadTaskRunner({}));
  }

  CupsPrintJobManagerImpl(const CupsPrintJobManagerImpl&) = delete;
  CupsPrintJobManagerImpl& operator=(const CupsPrintJobManagerImpl&) = delete;

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
    NOTREACHED_IN_MIGRATION() << "Pause printer is not implemented";
    return false;
  }

  bool ResumePrintJob(CupsPrintJob* job) override {
    NOTREACHED_IN_MIGRATION() << "Resume printer is not implemented";
    return false;
  }

  void OnDocDone(::printing::PrintJob* job,
                 ::printing::PrintedDocument* document,
                 int job_id) {
    // Store the printer data for metrics to be recorded upon print job status
    // updates.
    const std::string printer_id =
        base::UTF16ToUTF8(document->settings().device_name());
    const std::string key = CupsPrintJob::CreateUniqueId(printer_id, job_id);
    DCHECK(!printer_metrics_cache_.contains(key));
    printer_metrics_cache_.emplace(
        key, PrinterMetrics{job->settings().printer_manually_selected(),
                            job->settings().printer_status_reason()});

    // This event occurs after the print job has been successfully sent to the
    // spooler which is when we begin tracking the print queue.
    DCHECK(document);
    std::u16string title = ::printing::SimplifyDocumentTitle(document->name());
    if (title.empty()) {
      title = ::printing::SimplifyDocumentTitle(
          l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE));
    }

    // Calculate page total for given document to ensure UI displays the correct
    // count when document has copies.
    const int total_page_number = CalculatePrintJobTotalPages(document);
    CreatePrintJob(printer_id, base::UTF16ToUTF8(title), job_id,
                   total_page_number, job->source(), job->source_id(),
                   PrintSettingsToProto(document->settings()));
  }

  // Begin monitoring a print job for a given |printer_id| with the given
  // |title| with the pages |total_page_number|.
  bool CreatePrintJob(const std::string& printer_id,
                      const std::string& title,
                      uint32_t job_id,
                      int total_page_number,
                      ::printing::PrintJob::Source source,
                      const std::string& source_id,
                      const printing::proto::PrintSettings& settings) override {
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

    std::optional<chromeos::Printer> printer = manager->GetPrinter(printer_id);
    if (!printer) {
      LOG(WARNING)
          << "Printer was removed while job was in progress.  It cannot "
             "be tracked";
      return false;
    }

    // Record print job with scalable IPH framework.
    scalable_iph::ScalableIph* scalable_iph =
        ScalableIphFactory::GetForBrowserContext(profile);
    if (scalable_iph) {
      scalable_iph->RecordEvent(
          scalable_iph::ScalableIph::Event::kPrintJobCreated);
    }

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
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CupsPrintJobManagerImpl::PostQuery,
                                  weak_ptr_factory_.GetWeakPtr()));
    // Start the timer for ongoing queries.
    ScheduleQuery();

    return true;
  }

 private:
  void FinishPrintJob(CupsPrintJob* job) {
    // Copy job_id and printer_id.  |job| is about to be freed.
    const int job_id = job->job_id();
    const std::string printer_id = job->printer().id();

    // Stop monitoring jobs after we cancel them.  The user no longer cares.
    const std::string unique_id = job->GetUniqueId();
    jobs_.erase(unique_id);
    printer_metrics_cache_.erase(unique_id);

    cups_wrapper_->CancelJob(printer_id, job_id);
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

    cups_wrapper_->QueryCupsPrintJobs(
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

        const bool affected_by_ipp_usb_migration =
            print_job->printer().AffectedByIppUsbMigration();
        if (print_job->error_code() ==
            chromeos::PrinterErrorCode::CLIENT_UNAUTHORIZED) {
          // Job needs to be forcibly cancelled, CUPS will keep the job in held
          // and the job cannot be resumed in chromeos.
          FinishPrintJob(print_job);
          RecordJobResult(CLIENT_UNAUTHORIZED, affected_by_ipp_usb_migration);
        } else if (print_job->IsExpired()) {
          // Job needs to be forcibly cancelled.
          RecordJobResult(TIMEOUT_CANCEL, affected_by_ipp_usb_migration);
          FinishPrintJob(print_job);
          // Beware, print_job was removed from jobs_ and
          // deleted.
        } else if (print_job->PipelineDead()) {
          RecordJobResult(FILTER_FAILED, affected_by_ipp_usb_migration);
          FinishPrintJob(print_job);
        } else if (print_job->IsJobFinished()) {
          // Cleanup completed jobs.
          VLOG(1) << "Removing Job " << print_job->document_title();
          RecordJobResult(ResultForHistogram(print_job->state()),
                          affected_by_ipp_usb_migration);
          jobs_.erase(entry);
          printer_metrics_cache_.erase(key);
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
      CupsPrintJob* job = entry.second.get();
      RecordJobResult(LOST, job->printer().AffectedByIppUsbMigration());
      job->set_state(CupsPrintJob::State::STATE_FAILED);
      NotifyJobStateUpdate(job->GetWeakPtr());
    }

    jobs_.clear();
    printer_metrics_cache_.clear();
  }

  // Notify observers that a state update has occurred for |job|.
  void NotifyJobStateUpdate(base::WeakPtr<CupsPrintJob> job) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (!job)
      return;

    switch (job->state()) {
      case CupsPrintJob::State::STATE_NONE:
        // CupsPrintJob::State does not require notification.
        break;
      case CupsPrintJob::State::STATE_WAITING:
        NotifyJobUpdated(job);
        break;
      case CupsPrintJob::State::STATE_STARTED:
        NotifyJobStarted(job);
        break;
      case CupsPrintJob::State::STATE_PAGE_DONE:
        NotifyJobUpdated(job);
        break;
      case CupsPrintJob::State::STATE_RESUMED:
        NotifyJobResumed(job);
        break;
      case CupsPrintJob::State::STATE_SUSPENDED:
        NotifyJobSuspended(job);
        break;
      case CupsPrintJob::State::STATE_CANCELLED:
        NotifyJobCanceled(job);
        break;
      case CupsPrintJob::State::STATE_FAILED:
        NotifyJobFailed(job);
        break;
      case CupsPrintJob::State::STATE_DOCUMENT_DONE:
        NotifyJobDone(job);
        break;
      case CupsPrintJob::State::STATE_ERROR:
        NotifyJobUpdated(job);
        break;
    }

    RecordPrinterMetricIfJobComplete(job);
  }

  // Only record metrics for print jobs with states that denote success complete
  // or an error status.
  void RecordPrinterMetricIfJobComplete(base::WeakPtr<CupsPrintJob> job) {
    const std::string unique_id = job->GetUniqueId();
    auto iter = printer_metrics_cache_.find(unique_id);
    if (iter == printer_metrics_cache_.end()) {
      return;
    }

    bool print_job_success;
    switch (job->state()) {
      // Consider these print job statuses as "in progress" jobs so don't record
      // metrics yet.
      case CupsPrintJob::State::STATE_NONE:
      case CupsPrintJob::State::STATE_WAITING:
      case CupsPrintJob::State::STATE_STARTED:
      case CupsPrintJob::State::STATE_PAGE_DONE:
      case CupsPrintJob::State::STATE_RESUMED:
        return;
      // The set of states for a print job considered "failed" for metrics
      // recording.
      case CupsPrintJob::State::STATE_SUSPENDED:
      case CupsPrintJob::State::STATE_CANCELLED:
      case CupsPrintJob::State::STATE_FAILED:
      case CupsPrintJob::State::STATE_ERROR:
        print_job_success = false;
        break;
      case CupsPrintJob::State::STATE_DOCUMENT_DONE:
        print_job_success = true;
        break;
    }

    // Record PrintAttemptOutcome.
    const PrinterMetrics& metrics = iter->second;
    chromeos::PrintAttemptOutcome print_attempt_outcome;
    if (print_job_success && metrics.printer_manually_selected) {
      print_attempt_outcome = chromeos::PrintAttemptOutcome::
          kPrintJobSuccessManuallySelectedPrinter;
    } else if (print_job_success && !metrics.printer_manually_selected) {
      print_attempt_outcome =
          chromeos::PrintAttemptOutcome::kPrintJobSuccessInitialPrinter;
    } else if (!print_job_success && metrics.printer_manually_selected) {
      print_attempt_outcome =
          chromeos::PrintAttemptOutcome::kPrintJobFailManuallySelectedPrinter;
    } else {
      print_attempt_outcome =
          chromeos::PrintAttemptOutcome::kPrintJobFailInitialPrinter;
    }
    base::UmaHistogramEnumeration("PrintPreview.PrintAttemptOutcome",
                                  print_attempt_outcome);

    // Record printer status print job success.
    if (metrics.printer_status_reason.has_value()) {
      const std::string histogram_name = base::StrCat(
          {"PrintPreview.PrinterStatus.",
           GetPrinterStatusHistogramName(metrics.printer_status_reason.value()),
           ".PrintJobSuccess"});
      base::UmaHistogramBoolean(histogram_name, print_job_success);
    }

    printer_metrics_cache_.erase(unique_id);
  }

  // Maps the printer status reason to the variant names used for the
  // "PrintPreview.PrinterStatus.{StatusReason}.PrintJobSuccess" histogram.
  std::string GetPrinterStatusHistogramName(
      StatusReason printer_status_reason) {
    switch (printer_status_reason) {
      case StatusReason::kUnknownReason:
        return "UnknownReason";
      case StatusReason::kDeviceError:
        return "DeviceError";
      case StatusReason::kDoorOpen:
        return "DoorOpen";
      case StatusReason::kLowOnInk:
        return "LowOnInk";
      case StatusReason::kLowOnPaper:
        return "LowOnPaper";
      case StatusReason::kNoError:
        return "NoError";
      case StatusReason::kOutOfInk:
        return "OutOfInk";
      case StatusReason::kOutOfPaper:
        return "OutOfPaper";
      case StatusReason::kOutputAreaAlmostFull:
        return "OutputAreaAlmostFull";
      case StatusReason::kOutputFull:
        return "OutputFull";
      case StatusReason::kPaperJam:
        return "PaperJam";
      case StatusReason::kPaused:
        return "Paused";
      case StatusReason::kPrinterQueueFull:
        return "PrinterQueueFull";
      case StatusReason::kPrinterUnreachable:
        return "PrinterUnreachable";
      case StatusReason::kStopped:
        return "Stopped";
      case StatusReason::kTrayMissing:
        return "TrayMissing";
      case StatusReason::kExpiredCertificate:
        return "ExpiredCertificate";
    }
  }

  // Ongoing print jobs.
  std::map<std::string, std::unique_ptr<CupsPrintJob>> jobs_;

  // Records the number of consecutive times the GetJobs query has failed.
  int retry_count_ = 0;

  // Stores the desired printer metrics in a map keyed by the CupsPrintJob
  // `unique_id`. Once the corresponding print job either fails or completes,
  // record the metrics entry to histograms and remove it from the map.
  base::flat_map<std::string, PrinterMetrics> printer_metrics_cache_;

  base::RepeatingTimer timer_;
  std::unique_ptr<CupsWrapper> cups_wrapper_;
  ::printing::PrintJobManager::DocDoneCallbackList::Subscription subscription_;
  base::WeakPtrFactory<CupsPrintJobManagerImpl> weak_ptr_factory_;
};

// static
CupsPrintJobManager* CupsPrintJobManager::CreateInstance(Profile* profile) {
  return new CupsPrintJobManagerImpl(profile);
}

}  // namespace ash

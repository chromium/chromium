// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_manager_utils.h"

#include <algorithm>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "components/device_event_log/device_event_log.h"
#include "printing/backend/cups_jobs.h"
#include "printing/printed_document.h"
#include "printing/printer_status.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace ash {

namespace {

using ::chromeos::PrinterErrorCode;
using ::chromeos::PrinterErrorCodeFromPrinterStatusReasons;

// The amount of time elapsed from print job creation before a timeout is
// acknowledged. CUPS has a timeout of ~25s.
constexpr base::TimeDelta kMinElaspedPrintJobTimeout = base::Seconds(30);

// Returns the equivalent CupsPrintJob#State from a CupsJob#JobState.
CupsPrintJob::State ConvertState(::printing::CupsJob::JobState state) {
  switch (state) {
    case ::printing::CupsJob::PENDING:
      return CupsPrintJob::State::STATE_WAITING;
    case ::printing::CupsJob::HELD:
      return CupsPrintJob::State::STATE_SUSPENDED;
    case ::printing::CupsJob::PROCESSING:
      return CupsPrintJob::State::STATE_STARTED;
    case ::printing::CupsJob::CANCELED:
      return CupsPrintJob::State::STATE_CANCELLED;
    case ::printing::CupsJob::COMPLETED:
      return CupsPrintJob::State::STATE_DOCUMENT_DONE;
    case ::printing::CupsJob::STOPPED:
      return CupsPrintJob::State::STATE_SUSPENDED;
    case ::printing::CupsJob::ABORTED:
      return CupsPrintJob::State::STATE_FAILED;
    case ::printing::CupsJob::UNKNOWN:
      break;
  }

  NOTREACHED();
}

// Returns a string description of CUPS printer-state names
std::string_view PrinterStateName(ipp_pstate_t state) {
  switch (state) {
    case IPP_PSTATE_IDLE:
      return "idle";
    case IPP_PSTATE_PROCESSING:
      return "processing";
    case IPP_PSTATE_STOPPED:
      return "stopped";
    default:
      return "unknown";
  }
}

// Returns a string description of CUPS job-state names
std::string_view JobStateName(::printing::CupsJob::JobState state) {
  switch (state) {
    case ::printing::CupsJob::PENDING:
      return "pending";
    case ::printing::CupsJob::HELD:
      return "held";
    case ::printing::CupsJob::PROCESSING:
      return "processing";
    case ::printing::CupsJob::CANCELED:
      return "canceled";
    case ::printing::CupsJob::COMPLETED:
      return "completed";
    case ::printing::CupsJob::STOPPED:
      return "stopped";
    case ::printing::CupsJob::ABORTED:
      return "aborted";
    case ::printing::CupsJob::UNKNOWN:
      return "unknown";
  }

  NOTREACHED();
}

std::string GetStateDescription(const ::printing::PrinterStatus& printer_status,
                                const ::printing::CupsJob& job) {
  return base::StringPrintf("job=%s[%s] printer=%s[%s]",
                            JobStateName(job.state),
                            base::JoinString(job.state_reasons, ";"),
                            PrinterStateName(printer_status.state),
                            printer_status.AllReasonsAsString());
}

// Update the current printed page.  Returns true of the page has been updated.
bool UpdateCurrentPage(const ::printing::CupsJob& job,
                       CupsPrintJob* print_job) {
  bool pages_updated = false;
  if (job.current_pages <= 0 ||
      print_job->state() == CupsPrintJob::State::STATE_WAITING) {
    print_job->set_printed_page_number(std::max(job.current_pages, 0));
    print_job->set_state(CupsPrintJob::State::STATE_STARTED);
  } else {
    pages_updated = job.current_pages != print_job->printed_page_number();
    print_job->set_printed_page_number(job.current_pages);
    print_job->set_state(CupsPrintJob::State::STATE_PAGE_DONE);
  }

  return pages_updated;
}

void UpdateProcessingJob(const ::printing::PrinterStatus& printer_status,
                         const ::printing::CupsJob& job,
                         CupsPrintJob* print_job,
                         bool* pages_updated) {
  *pages_updated = UpdateCurrentPage(job, print_job);

  const PrinterErrorCode printer_error_code =
      PrinterErrorCodeFromPrinterStatusReasons(printer_status);
  const bool delay_print_job_timeout =
      printer_error_code == PrinterErrorCode::PRINTER_UNREACHABLE &&
      (base::Time::Now() - print_job->creation_time() <
       kMinElaspedPrintJobTimeout);

  if (printer_error_code != PrinterErrorCode::NO_ERROR &&
      !delay_print_job_timeout) {
    print_job->set_error_code(printer_error_code);
    print_job->set_state(printer_error_code ==
                                 PrinterErrorCode::PRINTER_UNREACHABLE
                             ? CupsPrintJob::State::STATE_FAILED
                             : CupsPrintJob::State::STATE_ERROR);
  } else {
    print_job->set_error_code(PrinterErrorCode::NO_ERROR);
  }
}

void UpdateCompletedJob(const ::printing::PrinterStatus& printer_status,
                        const ::printing::CupsJob& job,
                        CupsPrintJob* print_job) {
  if (job.ContainsStateReason(
          ::printing::CupsJob::JobStateReason::kJobCanceledByUser) ||
      job.ContainsStateReason(
          ::printing::CupsJob::JobStateReason::kJobCanceledByOperator) ||
      job.ContainsStateReason(
          ::printing::CupsJob::JobStateReason::kJobCanceledAtDevice)) {
    print_job->set_error_code(
        PrinterErrorCodeFromPrinterStatusReasons(printer_status));
    print_job->set_state(CupsPrintJob::State::STATE_CANCELLED);
    return;
  }

  if (job.current_pages < print_job->total_page_number() &&
      !job.ContainsStateReason(
          ::printing::CupsJob::JobStateReason::kJobCompletedSuccessfully)) {
    PRINTER_LOG(ERROR) << base::StringPrintf(
        "%s: job %d finished without completing all pages: %s", job.printer_id,
        job.id, job.state_message);
  }
  print_job->set_error_code(PrinterErrorCode::NO_ERROR);
  print_job->set_state(CupsPrintJob::State::STATE_DOCUMENT_DONE);
}

void UpdateStoppedJob(const ::printing::CupsJob& job, CupsPrintJob* print_job) {
  // If cups job STOPPED but with filter failure, treat as ERROR
  if (job.ContainsStateReason(
          ::printing::CupsJob::JobStateReason::kJobCompletedWithErrors)) {
    print_job->set_error_code(PrinterErrorCode::FILTER_FAILED);
    print_job->set_state(CupsPrintJob::State::STATE_FAILED);
  } else {
    print_job->set_error_code(PrinterErrorCode::NO_ERROR);
    print_job->set_state(ConvertState(job.state));
  }
}

void UpdateHeldJob(const ::printing::CupsJob& job, CupsPrintJob* print_job) {
  // If cups job STOPPED but with cups held for authentication, treat as ERROR
  if (job.ContainsStateReason(
          ::printing::CupsJob::JobStateReason::kCupsHeldForAuthentication)) {
    print_job->set_error_code(PrinterErrorCode::CLIENT_UNAUTHORIZED);
    print_job->set_state(CupsPrintJob::State::STATE_FAILED);
  } else {
    print_job->set_error_code(PrinterErrorCode::NO_ERROR);
    print_job->set_state(ConvertState(job.state));
  }
}

}  // namespace

bool UpdatePrintJob(const ::printing::PrinterStatus& printer_status,
                    const ::printing::CupsJob& job,
                    CupsPrintJob* print_job) {
  static absl::flat_hash_map<int, std::string> old_status;

  DCHECK_EQ(job.id, print_job->job_id());

  CupsPrintJob::State old_state = print_job->state();

  bool pages_updated = false;
  switch (job.state) {
    case ::printing::CupsJob::PROCESSING:
      UpdateProcessingJob(printer_status, job, print_job, &pages_updated);
      break;
    case ::printing::CupsJob::COMPLETED:
      UpdateCompletedJob(printer_status, job, print_job);
      break;
    case ::printing::CupsJob::STOPPED:
      UpdateStoppedJob(job, print_job);
      break;
    case ::printing::CupsJob::HELD:
      UpdateHeldJob(job, print_job);
      break;
    case ::printing::CupsJob::ABORTED:
    case ::printing::CupsJob::CANCELED:
      print_job->set_error_code(
          PrinterErrorCodeFromPrinterStatusReasons(printer_status));
      [[fallthrough]];
    default:
      print_job->set_state(ConvertState(job.state));
      break;
  }

  // If the status has changed since the last time the job was polled, log the
  // new status.  The printer-state-reasons and job-state-reasons can change
  // even if the job-state doesn't change, so this is based on matching the
  // previous status message instead of looking at just the state.
  bool updated = print_job->state() != old_state || pages_updated;
  if (!base::Contains(old_status, job.id)) {
    PRINTER_LOG(EVENT) << base::StringPrintf(
        "%s: job %d created with total_pages: %d, title: %s", job.printer_id,
        job.id, print_job->total_page_number(), print_job->document_title());
  }
  std::string status = base::StringPrintf(
      "%s: job %d changed to page %d/%d with state: %s", job.printer_id, job.id,
      print_job->printed_page_number(), print_job->total_page_number(),
      GetStateDescription(printer_status, job));
  if (status != old_status[job.id]) {
    if (updated) {
      PRINTER_LOG(EVENT) << status;
    } else {
      PRINTER_LOG(DEBUG) << status;
    }
    old_status[job.id] = status;
  }
  if (job.state == ::printing::CupsJob::COMPLETED ||
      job.state == ::printing::CupsJob::CANCELED ||
      job.state == ::printing::CupsJob::ABORTED) {
    PRINTER_LOG(EVENT) << base::StringPrintf(
        "%s: job %d finished in final state: %s", job.printer_id, job.id,
        JobStateName(job.state));

    // No need to save statuses for terminal states, since no more updates are
    // expected.
    old_status.erase(job.id);
  }

  return updated;
}

int CalculatePrintJobTotalPages(const ::printing::PrintedDocument* document) {
  if (document->settings().copies() == 0) {
    return document->page_count();
  }

  return document->page_count() * document->settings().copies();
}

}  // namespace ash

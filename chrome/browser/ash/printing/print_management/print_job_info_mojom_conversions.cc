// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_management/print_job_info_mojom_conversions.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace print_management {
namespace {

namespace mojom = ::chromeos::printing::printing_manager::mojom;

using ::chromeos::PrinterErrorCode;

mojom::PrintJobCompletionStatus PrintJobStatusProtoToMojom(
    proto::PrintJobInfo_PrintJobStatus print_job_status_proto) {
  switch (print_job_status_proto) {
    case proto::PrintJobInfo_PrintJobStatus_FAILED:
      return mojom::PrintJobCompletionStatus::kFailed;
    case proto::PrintJobInfo_PrintJobStatus_CANCELED:
      return mojom::PrintJobCompletionStatus::kCanceled;
    case proto::PrintJobInfo_PrintJobStatus_PRINTED:
      return mojom::PrintJobCompletionStatus::kPrinted;
    case proto::
        PrintJobInfo_PrintJobStatus_PrintJobInfo_PrintJobStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::
        PrintJobInfo_PrintJobStatus_PrintJobInfo_PrintJobStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED_IN_MIGRATION();
      return mojom::PrintJobCompletionStatus::kFailed;
  }
  return mojom::PrintJobCompletionStatus::kFailed;
}

mojom::ActivePrintJobState CupsPrintJobActiveStateToMojom(
    CupsPrintJob::State status) {
  switch (status) {
    case CupsPrintJob::State::STATE_NONE:
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      return mojom::ActivePrintJobState::kStarted;
    case CupsPrintJob::State::STATE_CANCELLED:
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
    case CupsPrintJob::State::STATE_FAILED:
    case CupsPrintJob::State::STATE_ERROR:
      return mojom::ActivePrintJobState::kDocumentDone;
  }
  NOTREACHED_IN_MIGRATION();
  return mojom::ActivePrintJobState::kDocumentDone;
}

mojom::PrinterErrorCode PrinterErrorCodeProtoToMojom(
    proto::PrintJobInfo_PrinterErrorCode printer_error_code_proto) {
  switch (printer_error_code_proto) {
    case proto::PrintJobInfo_PrinterErrorCode_NO_ERROR:
      return mojom::PrinterErrorCode::kNoError;
    case proto::PrintJobInfo_PrinterErrorCode_PAPER_JAM:
      return mojom::PrinterErrorCode::kPaperJam;
    case proto::PrintJobInfo_PrinterErrorCode_OUT_OF_PAPER:
      return mojom::PrinterErrorCode::kOutOfPaper;
    case proto::PrintJobInfo_PrinterErrorCode_OUT_OF_INK:
      return mojom::PrinterErrorCode::kOutOfInk;
    case proto::PrintJobInfo_PrinterErrorCode_DOOR_OPEN:
      return mojom::PrinterErrorCode::kDoorOpen;
    case proto::PrintJobInfo_PrinterErrorCode_PRINTER_UNREACHABLE:
      return mojom::PrinterErrorCode::kPrinterUnreachable;
    case proto::PrintJobInfo_PrinterErrorCode_TRAY_MISSING:
      return mojom::PrinterErrorCode::kTrayMissing;
    case proto::PrintJobInfo_PrinterErrorCode_OUTPUT_FULL:
      return mojom::PrinterErrorCode::kOutputFull;
    case proto::PrintJobInfo_PrinterErrorCode_STOPPED:
      return mojom::PrinterErrorCode::kStopped;
    case proto::PrintJobInfo_PrinterErrorCode_FILTER_FAILED:
      return mojom::PrinterErrorCode::kFilterFailed;
    case proto::PrintJobInfo_PrinterErrorCode_UNKNOWN_ERROR:
      return mojom::PrinterErrorCode::kUnknownError;
    case proto::PrintJobInfo_PrinterErrorCode_CLIENT_UNAUTHORIZED:
      return mojom::PrinterErrorCode::kClientUnauthorized;
    case proto::PrintJobInfo_PrinterErrorCode_EXPIRED_CERTIFICATE:
      return mojom::PrinterErrorCode::kExpiredCertificate;
    case proto::
        PrintJobInfo_PrinterErrorCode_PrintJobInfo_PrinterErrorCode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::
        PrintJobInfo_PrinterErrorCode_PrintJobInfo_PrinterErrorCode_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED_IN_MIGRATION();
      return mojom::PrinterErrorCode::kUnknownError;
  }
  return mojom::PrinterErrorCode::kUnknownError;
}

mojom::PrinterErrorCode PrinterErrorCodeToMojom(PrinterErrorCode error_code) {
  switch (error_code) {
    case PrinterErrorCode::NO_ERROR:
      return mojom::PrinterErrorCode::kNoError;
    case PrinterErrorCode::PAPER_JAM:
      return mojom::PrinterErrorCode::kPaperJam;
    case PrinterErrorCode::OUT_OF_PAPER:
      return mojom::PrinterErrorCode::kOutOfPaper;
    case PrinterErrorCode::OUT_OF_INK:
      return mojom::PrinterErrorCode::kOutOfInk;
    case PrinterErrorCode::DOOR_OPEN:
      return mojom::PrinterErrorCode::kDoorOpen;
    case PrinterErrorCode::PRINTER_UNREACHABLE:
      return mojom::PrinterErrorCode::kPrinterUnreachable;
    case PrinterErrorCode::TRAY_MISSING:
      return mojom::PrinterErrorCode::kTrayMissing;
    case PrinterErrorCode::OUTPUT_FULL:
      return mojom::PrinterErrorCode::kOutputFull;
    case PrinterErrorCode::STOPPED:
      return mojom::PrinterErrorCode::kStopped;
    case PrinterErrorCode::FILTER_FAILED:
      return mojom::PrinterErrorCode::kFilterFailed;
    case PrinterErrorCode::UNKNOWN_ERROR:
      return mojom::PrinterErrorCode::kUnknownError;
    case PrinterErrorCode::CLIENT_UNAUTHORIZED:
      return mojom::PrinterErrorCode::kClientUnauthorized;
    case PrinterErrorCode::EXPIRED_CERTIFICATE:
      return mojom::PrinterErrorCode::kExpiredCertificate;
  }
  return mojom::PrinterErrorCode::kUnknownError;
}

}  // namespace

mojom::PrintJobInfoPtr PrintJobProtoToMojom(
    const proto::PrintJobInfo& print_job_info_proto) {
  mojom::CompletedPrintJobInfoPtr completed_info_mojom =
      mojom::CompletedPrintJobInfo::New();

  completed_info_mojom->completion_status =
      PrintJobStatusProtoToMojom(print_job_info_proto.status());

  mojom::PrintJobInfoPtr print_job_mojom = mojom::PrintJobInfo::New();
  print_job_mojom->id = print_job_info_proto.id();
  print_job_mojom->title = base::UTF8ToUTF16(print_job_info_proto.title());
  print_job_mojom->creation_time = base::Time::FromMillisecondsSinceUnixEpoch(
      print_job_info_proto.creation_time());
  print_job_mojom->number_of_pages = print_job_info_proto.number_of_pages();
  print_job_mojom->printer_id = print_job_info_proto.printer().id();
  print_job_mojom->printer_name =
      base::UTF8ToUTF16(print_job_info_proto.printer().name());
  print_job_mojom->printer_uri = GURL(print_job_info_proto.printer().uri());
  print_job_mojom->printer_error_code =
      PrinterErrorCodeProtoToMojom(print_job_info_proto.printer_error_code());
  print_job_mojom->completed_info = std::move(completed_info_mojom);
  return print_job_mojom;
}

mojom::PrintJobInfoPtr CupsPrintJobToMojom(const CupsPrintJob& job) {
  mojom::ActivePrintJobInfoPtr active_job_info_mojom =
      mojom::ActivePrintJobInfo::New();
  active_job_info_mojom->active_state =
      CupsPrintJobActiveStateToMojom(job.state());
  active_job_info_mojom->printed_pages = job.printed_page_number();

  mojom::PrintJobInfoPtr print_job_mojom = mojom::PrintJobInfo::New();
  print_job_mojom->active_print_job_info = std::move(active_job_info_mojom);
  print_job_mojom->id = job.GetUniqueId();
  print_job_mojom->title = base::UTF8ToUTF16(job.document_title());
  print_job_mojom->creation_time = job.creation_time();
  print_job_mojom->number_of_pages = job.total_page_number();
  print_job_mojom->printer_id = job.printer().id();
  print_job_mojom->printer_name =
      base::UTF8ToUTF16(job.printer().display_name());
  print_job_mojom->printer_uri = GURL(job.printer().uri().GetNormalized());
  print_job_mojom->printer_error_code =
      PrinterErrorCodeToMojom(job.error_code());
  return print_job_mojom;
}

}  // namespace print_management
}  // namespace printing
}  // namespace ash

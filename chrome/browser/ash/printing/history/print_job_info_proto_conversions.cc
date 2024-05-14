// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_info_proto_conversions.h"

#include <optional>
#include <string>

#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "printing/mojom/print.mojom.h"

namespace ash {

namespace proto = printing::proto;

using ::chromeos::PrinterErrorCode;

namespace {

proto::PrintSettings_ColorMode ColorModelToProto(
    ::printing::mojom::ColorModel color) {
  std::optional<bool> is_color = ::printing::IsColorModelSelected(color);
  return is_color.value() ? proto::PrintSettings_ColorMode_COLOR
                          : proto::PrintSettings_ColorMode_BLACK_AND_WHITE;
}

proto::PrintSettings_DuplexMode DuplexModeToProto(
    ::printing::mojom::DuplexMode duplex) {
  switch (duplex) {
    case ::printing::mojom::DuplexMode::kSimplex:
      return proto::PrintSettings_DuplexMode_ONE_SIDED;
    case ::printing::mojom::DuplexMode::kLongEdge:
      return proto::PrintSettings_DuplexMode_TWO_SIDED_LONG_EDGE;
    case ::printing::mojom::DuplexMode::kShortEdge:
      return proto::PrintSettings_DuplexMode_TWO_SIDED_SHORT_EDGE;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return proto::PrintSettings_DuplexMode_ONE_SIDED;
}

proto::MediaSize RequestedMediaToProto(
    const ::printing::PrintSettings::RequestedMedia& media) {
  proto::MediaSize media_size_proto;
  media_size_proto.set_width(media.size_microns.width());
  media_size_proto.set_height(media.size_microns.height());
  media_size_proto.set_vendor_id(media.vendor_id);
  return media_size_proto;
}

proto::PrintJobInfo_PrintJobSource PrintJobSourceToProto(
    ::printing::PrintJob::Source source) {
  switch (source) {
    case ::printing::PrintJob::Source::kPrintPreview:
      return proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW;
    case ::printing::PrintJob::Source::kArc:
      return proto::PrintJobInfo_PrintJobSource_ARC;
    case ::printing::PrintJob::Source::kExtension:
      return proto::PrintJobInfo_PrintJobSource_EXTENSION;
    case ::printing::PrintJob::Source::kPrintPreviewIncognito:
      return proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW_INCOGNITO;
    case ::printing::PrintJob::Source::kIsolatedWebApp:
      return proto::PrintJobInfo_PrintJobSource_ISOLATED_WEB_APP;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW;
}

proto::PrintJobInfo_PrintJobStatus PrintJobStateToProto(
    CupsPrintJob::State state) {
  switch (state) {
    case CupsPrintJob::State::STATE_FAILED:
      return proto::PrintJobInfo_PrintJobStatus_FAILED;
    case CupsPrintJob::State::STATE_CANCELLED:
      return proto::PrintJobInfo_PrintJobStatus_CANCELED;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      return proto::PrintJobInfo_PrintJobStatus_PRINTED;
    // Only completed print jobs are saved in the database so we shouldn't
    // handle other states.
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return proto::PrintJobInfo_PrintJobStatus_CANCELED;
}

proto::Printer_PrinterSource PrinterSourceToProto(
    chromeos::Printer::Source source) {
  switch (source) {
    case chromeos::Printer::Source::SRC_USER_PREFS:
      return proto::Printer_PrinterSource_USER;
    case chromeos::Printer::Source::SRC_POLICY:
      return proto::Printer_PrinterSource_POLICY;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return proto::Printer_PrinterSource_USER;
}

proto::PrintJobInfo_PrinterErrorCode PrinterErrorCodeToProto(
    PrinterErrorCode error_code) {
  switch (error_code) {
    case PrinterErrorCode::NO_ERROR:
      return proto::PrintJobInfo_PrinterErrorCode_NO_ERROR;
    case PrinterErrorCode::PAPER_JAM:
      return proto::PrintJobInfo_PrinterErrorCode_PAPER_JAM;
    case PrinterErrorCode::OUT_OF_PAPER:
      return proto::PrintJobInfo_PrinterErrorCode_OUT_OF_PAPER;
    case PrinterErrorCode::OUT_OF_INK:
      return proto::PrintJobInfo_PrinterErrorCode_OUT_OF_INK;
    case PrinterErrorCode::DOOR_OPEN:
      return proto::PrintJobInfo_PrinterErrorCode_DOOR_OPEN;
    case PrinterErrorCode::PRINTER_UNREACHABLE:
      return proto::PrintJobInfo_PrinterErrorCode_PRINTER_UNREACHABLE;
    case PrinterErrorCode::TRAY_MISSING:
      return proto::PrintJobInfo_PrinterErrorCode_TRAY_MISSING;
    case PrinterErrorCode::OUTPUT_FULL:
      return proto::PrintJobInfo_PrinterErrorCode_OUTPUT_FULL;
    case PrinterErrorCode::STOPPED:
      return proto::PrintJobInfo_PrinterErrorCode_STOPPED;
    case PrinterErrorCode::FILTER_FAILED:
      return proto::PrintJobInfo_PrinterErrorCode_FILTER_FAILED;
    case PrinterErrorCode::UNKNOWN_ERROR:
      return proto::PrintJobInfo_PrinterErrorCode_UNKNOWN_ERROR;
    case PrinterErrorCode::CLIENT_UNAUTHORIZED:
      return proto::PrintJobInfo_PrinterErrorCode_CLIENT_UNAUTHORIZED;
    case PrinterErrorCode::EXPIRED_CERTIFICATE:
      return proto::PrintJobInfo_PrinterErrorCode_EXPIRED_CERTIFICATE;
    default:
      // Be sure to update the above case statements whenever a new printer
      // error is introduced.
      NOTREACHED_IN_MIGRATION();
  }
  return proto::PrintJobInfo_PrinterErrorCode_UNKNOWN_ERROR;
}

// Helper method to convert base::Time to the number of milliseconds past the
// Unix epoch. Loses precision beyond milliseconds.
int64_t TimeToMillisecondsPastUnixEpoch(const base::Time& time) {
  return static_cast<int64_t>(time.InMillisecondsFSinceUnixEpoch());
}

proto::Printer PrinterToProto(const chromeos::Printer& printer) {
  proto::Printer printer_proto;
  printer_proto.set_id(printer.id());
  printer_proto.set_name(printer.display_name());
  printer_proto.set_uri(printer.uri().GetNormalized());
  printer_proto.set_source(PrinterSourceToProto(printer.source()));
  return printer_proto;
}

}  // namespace

proto::PrintSettings PrintSettingsToProto(
    const ::printing::PrintSettings& settings) {
  proto::PrintSettings settings_proto;
  settings_proto.set_color(ColorModelToProto(settings.color()));
  settings_proto.set_duplex(DuplexModeToProto(settings.duplex_mode()));
  *settings_proto.mutable_media_size() =
      RequestedMediaToProto(settings.requested_media());
  settings_proto.set_copies(settings.copies());
  return settings_proto;
}

proto::PrintJobInfo CupsPrintJobToProto(const CupsPrintJob& print_job,
                                        const std::string& id,
                                        const base::Time& completion_time) {
  proto::PrintJobInfo print_job_info_proto;
  print_job_info_proto.set_id(id);
  print_job_info_proto.set_title(print_job.document_title());
  print_job_info_proto.set_source(PrintJobSourceToProto(print_job.source()));
  print_job_info_proto.set_source_id(print_job.source_id());
  print_job_info_proto.set_status(PrintJobStateToProto(print_job.state()));
  print_job_info_proto.set_printer_error_code(
      PrinterErrorCodeToProto(print_job.error_code()));
  print_job_info_proto.set_creation_time(
      TimeToMillisecondsPastUnixEpoch(print_job.creation_time()));
  print_job_info_proto.set_completion_time(
      TimeToMillisecondsPastUnixEpoch(completion_time));
  *print_job_info_proto.mutable_printer() = PrinterToProto(print_job.printer());
  *print_job_info_proto.mutable_settings() = print_job.settings();
  print_job_info_proto.set_number_of_pages(print_job.total_page_number());
  return print_job_info_proto;
}

}  // namespace ash

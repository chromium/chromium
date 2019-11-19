// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_info_proto_conversions.h"

namespace chromeos {

namespace proto = printing::proto;

namespace {

proto::PrintSettings_ColorMode ColorModelToProto(::printing::ColorModel color) {
  return ::printing::IsColorModelSelected(color)
             ? proto::PrintSettings_ColorMode_COLOR
             : proto::PrintSettings_ColorMode_BLACK_AND_WHITE;
}

proto::PrintSettings_DuplexMode DuplexModeToProto(
    ::printing::DuplexMode duplex) {
  switch (duplex) {
    case ::printing::DuplexMode::SIMPLEX:
      return proto::PrintSettings_DuplexMode_ONE_SIDED;
    case ::printing::DuplexMode::LONG_EDGE:
      return proto::PrintSettings_DuplexMode_TWO_SIDED_LONG_EDGE;
    case ::printing::DuplexMode::SHORT_EDGE:
      return proto::PrintSettings_DuplexMode_TWO_SIDED_SHORT_EDGE;
    default:
      NOTREACHED();
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
    case ::printing::PrintJob::Source::PRINT_PREVIEW:
      return proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW;
    case ::printing::PrintJob::Source::ARC:
      return proto::PrintJobInfo_PrintJobSource_ARC;
    default:
      NOTREACHED();
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
      NOTREACHED();
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
      NOTREACHED();
  }
  return proto::Printer_PrinterSource_USER;
}

// Helper method to convert base::Time to the number of milliseconds past the
// Unix epoch. Loses precision beyond milliseconds.
int64_t TimeToMillisecondsPastUnixEpoch(const base::Time& time) {
  return static_cast<int64_t>(time.ToJsTime());
}

proto::Printer PrinterToProto(const chromeos::Printer& printer) {
  proto::Printer printer_proto;
  printer_proto.set_name(printer.display_name());
  printer_proto.set_uri(printer.uri());
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
  print_job_info_proto.set_creation_time(
      TimeToMillisecondsPastUnixEpoch(print_job.creation_time()));
  print_job_info_proto.set_completion_time(
      TimeToMillisecondsPastUnixEpoch(completion_time));
  *print_job_info_proto.mutable_printer() = PrinterToProto(print_job.printer());
  *print_job_info_proto.mutable_settings() = print_job.settings();
  print_job_info_proto.set_number_of_pages(print_job.total_page_number());
  return print_job_info_proto;
}

}  // namespace chromeos

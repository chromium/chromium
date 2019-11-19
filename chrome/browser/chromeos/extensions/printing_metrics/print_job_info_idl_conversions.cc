// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_info_idl_conversions.h"

#include "base/numerics/safe_conversions.h"

namespace proto = chromeos::printing::proto;

namespace extensions {

namespace idl = api::printing_metrics;

namespace {

idl::ColorMode ColorModeProtoToIdl(proto::PrintSettings_ColorMode color_proto) {
  switch (color_proto) {
    case proto::PrintSettings_ColorMode_BLACK_AND_WHITE:
      return idl::COLOR_MODE_BLACK_AND_WHITE;
    case proto::PrintSettings_ColorMode_COLOR:
      return idl::COLOR_MODE_COLOR;
    default:
      NOTREACHED();
  }
  return idl::COLOR_MODE_BLACK_AND_WHITE;
}

idl::DuplexMode DuplexModeProtoToIdl(
    proto::PrintSettings_DuplexMode duplex_proto) {
  switch (duplex_proto) {
    case proto::PrintSettings_DuplexMode_ONE_SIDED:
      return idl::DUPLEX_MODE_ONE_SIDED;
    case proto::PrintSettings_DuplexMode_TWO_SIDED_LONG_EDGE:
      return idl::DUPLEX_MODE_TWO_SIDED_LONG_EDGE;
    case proto::PrintSettings_DuplexMode_TWO_SIDED_SHORT_EDGE:
      return idl::DUPLEX_MODE_TWO_SIDED_SHORT_EDGE;
    default:
      NOTREACHED();
  }
  return idl::DUPLEX_MODE_ONE_SIDED;
}

idl::MediaSize MediaSizeProtoToIdl(const proto::MediaSize& media_size_proto) {
  idl::MediaSize media_size;
  media_size.width = media_size_proto.width();
  media_size.height = media_size_proto.height();
  media_size.vendor_id = media_size_proto.vendor_id();
  return media_size;
}

idl::PrintSettings PrintSettingsProtoToIdl(
    const proto::PrintSettings& settings_proto) {
  idl::PrintSettings settings;
  settings.color = ColorModeProtoToIdl(settings_proto.color());
  settings.duplex = DuplexModeProtoToIdl(settings_proto.duplex());
  settings.media_size = MediaSizeProtoToIdl(settings_proto.media_size());
  settings.copies = settings_proto.copies();
  return settings;
}

idl::PrintJobSource PrintJobSourceProtoToIdl(
    proto::PrintJobInfo_PrintJobSource print_job_source_proto) {
  switch (print_job_source_proto) {
    case proto::PrintJobInfo_PrintJobSource_PRINT_PREVIEW:
      return idl::PRINT_JOB_SOURCE_PRINT_PREVIEW;
    case proto::PrintJobInfo_PrintJobSource_ARC:
      return idl::PRINT_JOB_SOURCE_ANDROID_APP;
    default:
      NOTREACHED();
  }
  return idl::PRINT_JOB_SOURCE_PRINT_PREVIEW;
}

idl::PrintJobStatus PrintJobStatusProtoToIdl(
    proto::PrintJobInfo_PrintJobStatus print_job_status_proto) {
  switch (print_job_status_proto) {
    case proto::PrintJobInfo_PrintJobStatus_FAILED:
      return idl::PRINT_JOB_STATUS_FAILED;
    case proto::PrintJobInfo_PrintJobStatus_CANCELED:
      return idl::PRINT_JOB_STATUS_CANCELED;
    case proto::PrintJobInfo_PrintJobStatus_PRINTED:
      return idl::PRINT_JOB_STATUS_PRINTED;
    default:
      NOTREACHED();
  }
  return idl::PRINT_JOB_STATUS_FAILED;
}

idl::PrinterSource PrinterSourceProtoToIdl(
    proto::Printer_PrinterSource printer_source_proto) {
  switch (printer_source_proto) {
    case proto::Printer_PrinterSource_USER:
      return idl::PRINTER_SOURCE_USER;
    case proto::Printer_PrinterSource_POLICY:
      return idl::PRINTER_SOURCE_POLICY;
    default:
      NOTREACHED();
  }
  return idl::PRINTER_SOURCE_USER;
}

idl::Printer PrinterProtoToIdl(const proto::Printer& printer_proto) {
  idl::Printer printer;
  printer.name = printer_proto.name();
  printer.uri = printer_proto.uri();
  printer.source = PrinterSourceProtoToIdl(printer_proto.source());
  return printer;
}

}  // namespace

idl::PrintJobInfo PrintJobInfoProtoToIdl(
    const proto::PrintJobInfo& print_job_info_proto) {
  idl::PrintJobInfo print_job_info;
  print_job_info.id = print_job_info_proto.id();
  print_job_info.title = print_job_info_proto.title();
  print_job_info.source =
      PrintJobSourceProtoToIdl(print_job_info_proto.source());
  print_job_info.source_id = nullptr;
  print_job_info.status =
      PrintJobStatusProtoToIdl(print_job_info_proto.status());
  print_job_info.creation_time =
      base::checked_cast<double>(print_job_info_proto.creation_time());
  print_job_info.completion_time =
      base::checked_cast<double>(print_job_info_proto.completion_time());
  print_job_info.printer = PrinterProtoToIdl(print_job_info_proto.printer());
  print_job_info.settings =
      PrintSettingsProtoToIdl(print_job_info_proto.settings());
  print_job_info.number_of_pages = print_job_info_proto.number_of_pages();
  return print_job_info;
}

}  // namespace extensions
